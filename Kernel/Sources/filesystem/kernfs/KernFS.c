//
//  KernFS.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "KernFSPriv.h"
#include "KfsDirectory.h"
#include "KfsDevice.h"
#include <driver/Driver.h>
#include <filesystem/DirectoryChannel.h>


// Creates an instance of KernFS.
errno_t KernFS_Create(KernFSRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    KernFSRef self;

    try(Filesystem_Create(&kKernFSClass, (FilesystemRef*)&self));
    try(FSAllocateCleared(sizeof(List) * IN_HASH_CHAINS_COUNT, (void**)&self->inOwned));

    Lock_Init(&self->inOwnedLock);
    self->nextAvailableInodeId = 1;

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

void KernFS_deinit(KernFSRef _Nonnull self)
{
    for (size_t i = 0; i < IN_HASH_CHAINS_COUNT; i++) {
        List_ForEach(&self->inOwned[i], struct Inode,
            Inode_Destroy((InodeRef)KfsNodeFromHashChainPointer(pCurNode));
        )
    }
    Lock_Deinit(&self->inOwnedLock);
}

ino_t KernFS_GetNextAvailableInodeId(KernFSRef _Nonnull self)
{
    Lock_Lock(&self->inOwnedLock);
    ino_t id = self->nextAvailableInodeId;
    self->nextAvailableInodeId++;
    Lock_Unlock(&self->inOwnedLock);
    
    return id;
}

void _KernFS_AddInode(KernFSRef _Nonnull self, KfsNodeRef _Nonnull ip)
{
    const size_t idx = IN_HASH_INDEX(Inode_GetId(ip));

    Lock_Lock(&self->inOwnedLock);
    List_InsertBeforeFirst(&self->inOwned[idx], &ip->inChain);
    Lock_Unlock(&self->inOwnedLock);
}

void _KernFS_DestroyInode(KernFSRef _Nonnull self, KfsNodeRef _Nonnull ip)
{
    const ino_t id = Inode_GetId(ip);
    const size_t idx = IN_HASH_INDEX(id);

    Lock_Lock(&self->inOwnedLock);
    List_ForEach(&self->inOwned[idx], struct KfsNode,
        KfsNodeRef curNode = KfsNodeFromHashChainPointer(pCurNode);

        if (Inode_GetId(curNode) == id) {
            List_Remove(&self->inOwned[idx], &ip->inChain);
            Inode_Destroy((InodeRef)ip);
            break;
        }
    )
    Lock_Unlock(&self->inOwnedLock);
}

KfsNodeRef _Nullable _KernFS_GetInode(KernFSRef _Nonnull self, ino_t id)
{
    const size_t idx = IN_HASH_INDEX(id);
    KfsNodeRef theNode = NULL;

    Lock_Lock(&self->inOwnedLock);
    List_ForEach(&self->inOwned[idx], struct KfsNode,
        KfsNodeRef curNode = KfsNodeFromHashChainPointer(pCurNode);

        if (Inode_GetId(curNode) == id) {
            theNode = curNode;
            break;
        }
    )
    Lock_Unlock(&self->inOwnedLock);

    return theNode;
}


errno_t KernFS_onStart(KernFSRef _Nonnull self, const void* _Nonnull pParams, ssize_t paramsSize, FSProperties* _Nonnull pOutProps)
{
    decl_try_err();
    KfsNodeRef rootDir;

    const FilePermissions dirOwnerPerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions dirOtherPerms = kFilePermission_Read | kFilePermission_Execute;
    const FilePermissions rootDirPerms = FilePermissions_Make(dirOwnerPerms, dirOtherPerms, dirOtherPerms);
    const ino_t rootDirInodeId = KernFS_GetNextAvailableInodeId(self);

    try(KfsDirectory_Create(self, rootDirInodeId, rootDirPerms, kUserId_Root, kGroupId_Root, rootDirInodeId, &rootDir));
    _KernFS_AddInode(self, rootDir);
    
    pOutProps->rootDirectoryId = rootDirInodeId;
    pOutProps->isReadOnly = false;

catch:
    return err;
}

errno_t KernFS_getInfo(KernFSRef _Nonnull self, FSInfo* _Nonnull pOutInfo)
{
    memset(pOutInfo, 0, sizeof(FSInfo));
    pOutInfo->fsid = Filesystem_GetId(self);
    pOutInfo->mediaId = 1;
    pOutInfo->properties |= kFSProperty_IsCatalog;
    return EOK;
}

static errno_t KernFS_unlinkCore(KernFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pNodeToUnlink, InodeRef _Nonnull _Locked pDir)
{
    decl_try_err();

    // Remove the directory entry in the parent directory
    try(KfsDirectory_RemoveEntry((KfsDirectoryRef)pDir, pNodeToUnlink));


    // Unlink the node itself
    Inode_Unlink(pNodeToUnlink);
    Inode_SetModified(pNodeToUnlink, kInodeFlag_StatusChanged);

catch:
    return err;
}

// Unlink the node 'pNode' which is an immediate child of 'pParentNode'.
// Both nodes are guaranteed to be members of the same filesystem. 'pNode'
// is guaranteed to exist and that it isn't a mountpoint and not the root
// node of the filesystem.
// This function must validate that that if 'pNode' is a directory, that the
// directory is empty (contains nothing except "." and "..").
errno_t KernFS_unlink(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked target, InodeRef _Nonnull _Locked dir)
{
    decl_try_err();

    // A directory must be empty in order to be allowed to unlink it
    if (Inode_IsDirectory(target) && Inode_GetLinkCount(target) > 1 && KfsDirectory_IsEmpty((KfsDirectoryRef)target)) {
        throw(EBUSY);
    }


    try(KernFS_unlinkCore(self, target, dir));
    Inode_Writeback(dir);

catch:
    return err;
}

errno_t KernFS_link(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pName, uid_t uid, gid_t gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    decl_try_err();

    try(KfsDirectory_InsertEntry((KfsDirectoryRef)pDstDir, Inode_GetId(pSrcNode), Inode_IsDirectory(pSrcNode), pName));
    Inode_Writeback(pDstDir);

    Inode_Link(pSrcNode);
    Inode_SetModified(pSrcNode, kInodeFlag_StatusChanged);
    
catch:
    return err;
}

errno_t KernFS_move(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pNewName, uid_t uid, gid_t gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    return EPERM;
}

errno_t KernFS_rename(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, const PathComponent* _Nonnull pNewName, uid_t uid, gid_t gid)
{
    return EPERM;
}


class_func_defs(KernFS, Filesystem,
override_func_def(deinit, KernFS, Object)
override_func_def(onStart, KernFS, Filesystem)
override_func_def(getInfo, KernFS, Filesystem)
override_func_def(onAcquireNode, KernFS, Filesystem)
override_func_def(onWritebackNode, KernFS, Filesystem)
override_func_def(onRelinquishNode, KernFS, Filesystem)
override_func_def(acquireNodeForName, KernFS, Filesystem)
override_func_def(getNameOfNode, KernFS, Filesystem)
override_func_def(createNode, KernFS, Filesystem)
override_func_def(unlink, KernFS, Filesystem)
override_func_def(move, KernFS, Filesystem)
override_func_def(rename, KernFS, Filesystem)
);
