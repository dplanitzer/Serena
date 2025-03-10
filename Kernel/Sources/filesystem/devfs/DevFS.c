//
//  DevFS.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DevFSPriv.h"
#include "DfsDirectory.h"
#include "DfsDevice.h"
#include <driver/Driver.h>
#include <filesystem/DirectoryChannel.h>


// Creates an instance of DevFS.
errno_t DevFS_Create(DevFSRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DevFSRef self;
    DfsNodeRef rootDir = NULL;

    try(Filesystem_Create(&kDevFSClass, (FilesystemRef*)&self));
    try(FSAllocateCleared(sizeof(List) * IN_HASH_CHAINS_COUNT, (void**)&self->inOwned));

    SELock_Init(&self->seLock);
    self->nextAvailableInodeId = 1;

    const FilePermissions dirOwnerPerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions dirOtherPerms = kFilePermission_Read | kFilePermission_Execute;
    const FilePermissions rootDirPerms = FilePermissions_Make(dirOwnerPerms, dirOtherPerms, dirOtherPerms);
    self->rootDirInodeId = DevFS_GetNextAvailableInodeId(self);

    try(DfsDirectory_Create(self, self->rootDirInodeId, rootDirPerms, kUserId_Root, kGroupId_Root, self->rootDirInodeId, &rootDir));
    _DevFS_AddInode(self, rootDir);

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void DevFS_deinit(DevFSRef _Nonnull self)
{
    if (!Filesystem_CanUnmount((FilesystemRef)self)) {
        // This should never happen because a filesystem can not be destroyed
        // as long as inodes are still alive
        abort();
    }

    for (size_t i = 0; i < IN_HASH_CHAINS_COUNT; i++) {
        List_ForEach(&self->inOwned[i], struct Inode,
            Inode_Destroy((InodeRef)DfsNodeFromHashChainPointer(pCurNode));
        )
    }
    SELock_Deinit(&self->seLock);
}

errno_t DevFS_start(DevFSRef _Nonnull self, const void* _Nonnull pParams, ssize_t paramsSize)
{
    decl_try_err();

    if ((err = SELock_LockExclusive(&self->seLock)) != EOK) {
        return err;
    }

    if (self->flags.isMounted) {
        throw(EIO);
    }

    self->flags.isMounted = 1;

catch:
    SELock_Unlock(&self->seLock);
    return err;
}

errno_t DevFS_stop(DevFSRef _Nonnull self)
{
    decl_try_err();

    if ((err = SELock_LockExclusive(&self->seLock)) != EOK) {
        return err;
    }
    if (!self->flags.isMounted) {
        throw(EIO);
    }
    if (!Filesystem_CanUnmount((FilesystemRef)self)) {
        throw(EBUSY);
    }

    self->flags.isMounted = 0;

catch:
    SELock_Unlock(&self->seLock);
    return err;
}

ino_t DevFS_GetNextAvailableInodeId(DevFSRef _Nonnull _Locked self)
{
    ino_t id = self->nextAvailableInodeId;

    self->nextAvailableInodeId++;
    return id;
}

void _DevFS_AddInode(DevFSRef _Nonnull _Locked self, DfsNodeRef _Nonnull ip)
{
    const size_t idx = IN_HASH_INDEX(Inode_GetId(ip));

    List_InsertBeforeFirst(&self->inOwned[idx], &ip->inChain);
}

void _DevFS_DestroyInode(DevFSRef _Nonnull _Locked self, DfsNodeRef _Nonnull ip)
{
    const ino_t id = Inode_GetId(ip);
    const size_t idx = IN_HASH_INDEX(id);

    List_ForEach(&self->inOwned[idx], struct DfsNode,
        DfsNodeRef curNode = DfsNodeFromHashChainPointer(pCurNode);

        if (Inode_GetId(curNode) == id) {
            List_Remove(&self->inOwned[idx], &ip->inChain);
            Inode_Destroy((InodeRef)ip);
            return;
        }
    )
}

DfsNodeRef _Nullable _DevFS_GetInode(DevFSRef _Nonnull _Locked self, ino_t id)
{
    const size_t idx = IN_HASH_INDEX(id);

    List_ForEach(&self->inOwned[idx], struct DfsNode,
        DfsNodeRef curNode = DfsNodeFromHashChainPointer(pCurNode);

        if (Inode_GetId(curNode) == id) {
            return curNode;
        }
    )

    return NULL;
}


bool DevFS_isReadOnly(DevFSRef _Nonnull self)
{
    return false;
}

static errno_t DevFS_unlinkCore(DevFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pNodeToUnlink, InodeRef _Nonnull _Locked pDir)
{
    decl_try_err();

    // Remove the directory entry in the parent directory
    try(DfsDirectory_RemoveEntry((DfsDirectoryRef)pDir, pNodeToUnlink));


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
errno_t DevFS_unlink(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked target, InodeRef _Nonnull _Locked dir)
{
    decl_try_err();

    try_bang(SELock_LockExclusive(&self->seLock));

    // A directory must be empty in order to be allowed to unlink it
    if (Inode_IsDirectory(target) && Inode_GetLinkCount(target) > 1 && DfsDirectory_IsEmpty((DfsDirectoryRef)target)) {
        throw(EBUSY);
    }


    try(DevFS_unlinkCore(self, target, dir));
    Inode_Writeback(dir);

catch:
    SELock_Unlock(&self->seLock);
    return err;
}

errno_t DevFS_link(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pName, uid_t uid, gid_t gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    decl_try_err();

    try_bang(SELock_LockExclusive(&self->seLock));
    try(DfsDirectory_InsertEntry((DfsDirectoryRef)pDstDir, Inode_GetId(pSrcNode), Inode_IsDirectory(pSrcNode), pName));
    Inode_Writeback(pDstDir);

    Inode_Link(pSrcNode);
    Inode_SetModified(pSrcNode, kInodeFlag_StatusChanged);
    
catch:
    SELock_Unlock(&self->seLock);
    return err;
}

errno_t DevFS_move(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pNewName, uid_t uid, gid_t gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    return EPERM;
}

errno_t DevFS_rename(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, const PathComponent* _Nonnull pNewName, uid_t uid, gid_t gid)
{
    return EPERM;
}


class_func_defs(DevFS, Filesystem,
override_func_def(deinit, DevFS, Object)
override_func_def(onAcquireNode, DevFS, Filesystem)
override_func_def(onWritebackNode, DevFS, Filesystem)
override_func_def(onRelinquishNode, DevFS, Filesystem)
override_func_def(start, DevFS, Filesystem)
override_func_def(stop, DevFS, Filesystem)
override_func_def(isReadOnly, DevFS, Filesystem)
override_func_def(acquireRootDirectory, DevFS, Filesystem)
override_func_def(acquireNodeForName, DevFS, Filesystem)
override_func_def(getNameOfNode, DevFS, Filesystem)
override_func_def(createNode, DevFS, Filesystem)
override_func_def(unlink, DevFS, Filesystem)
override_func_def(move, DevFS, Filesystem)
override_func_def(rename, DevFS, Filesystem)
);
