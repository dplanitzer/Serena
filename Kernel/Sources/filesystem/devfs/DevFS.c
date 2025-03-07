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
    DfsDirectoryItem* rootDir = NULL;

    try(Filesystem_Create(&kDevFSClass, (FilesystemRef*)&self));
    SELock_Init(&self->seLock);
    self->nextAvailableInodeId = 1;

    for (size_t i = 0; i < INID_HASH_CHAINS_COUNT; i++) {
        List_Init(&self->inidChains[i]);
    }

    const FilePermissions dirOwnerPerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions dirOtherPerms = kFilePermission_Read | kFilePermission_Execute;
    const FilePermissions rootDirPerms = FilePermissions_Make(dirOwnerPerms, dirOtherPerms, dirOtherPerms);
    const ino_t rootDirInid = DevFS_GetNextAvailableInodeId(self);

    try(DfsDirectoryItem_Create(rootDirInid, rootDirPerms, kUserId_Root, kGroupId_Root, rootDirInid, &rootDir));
    DevFS_AddItem(self, (DfsItem*)rootDir);
    self->rootDirInodeId = rootDirInid;

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void DevFS_deinit(DevFSRef _Nonnull self)
{
    for (size_t i = 0; i < INID_HASH_CHAINS_COUNT; i++) {
        List_ForEach(&self->inidChains[i], DfsItem,
            DfsItem_Destroy(pCurNode);
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

void DevFS_AddItem(DevFSRef _Nonnull _Locked self, DfsItem* _Nonnull item)
{
    const size_t idx = INID_HASH_INDEX(item->inid);

    List_InsertAfterLast(&self->inidChains[idx], &item->inidChain);
}

void DevFS_RemoveItem(DevFSRef _Nonnull _Locked self, ino_t inid)
{
    const size_t idx = INID_HASH_INDEX(inid);

    List_ForEach(&self->inidChains[idx], DfsItem,
        if (pCurNode->inid == inid) {
            List_Remove(&self->inidChains[idx], &pCurNode->inidChain);
            return;
        }
    )
}

DfsItem* _Nullable DevFS_GetItem(DevFSRef _Nonnull _Locked self, ino_t inid)
{
    const size_t idx = INID_HASH_INDEX(inid);

    List_ForEach(&self->inidChains[idx], DfsItem,
        if (pCurNode->inid == inid) {
            return pCurNode;
        }
    )
}


bool DevFS_isReadOnly(DevFSRef _Nonnull self)
{
    return false;
}

static errno_t DevFS_unlinkCore(DevFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pNodeToUnlink, InodeRef _Nonnull _Locked pDir)
{
    decl_try_err();

    // Remove the directory entry in the parent directory
    try(DevFS_RemoveDirectoryEntry(self, pDir, Inode_GetId(pNodeToUnlink)));


    // If this is a directory then unlink it from its parent since we remove a
    // '..' entry that points to the parent
    if (Inode_IsDirectory(pNodeToUnlink)) {
        Inode_Unlink(pDir);
    }


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
    if (Inode_IsDirectory(target) && Inode_GetLinkCount(target) > 1 && DfsDirectoryItem_IsEmpty(DfsDirectory_GetItem(target))) {
        throw(EBUSY);
    }


    try(DevFS_unlinkCore(self, target, dir));

catch:
    SELock_Unlock(&self->seLock);
    return err;
}

errno_t DevFS_link(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pName, uid_t uid, gid_t gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    decl_try_err();

    try_bang(SELock_LockExclusive(&self->seLock));
    try(DevFS_InsertDirectoryEntry(self, pDstDir, Inode_GetId(pSrcNode), pName));
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
