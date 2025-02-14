//
//  DevFS.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DevFSPriv.h"
#include <driver/Driver.h>
#include <filesystem/DirectoryChannel.h>
#include <security/SecurityManager.h>


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
    const InodeId rootDirInid = DevFS_GetNextAvailableInodeId(self);

    try(DfsDirectoryItem_Create(rootDirInid, rootDirPerms, kRootUserId, kRootGroupId, rootDirInid, &rootDir));
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

InodeId DevFS_GetNextAvailableInodeId(DevFSRef _Nonnull _Locked self)
{
    InodeId id = self->nextAvailableInodeId;

    self->nextAvailableInodeId++;
    return id;
}

void DevFS_AddItem(DevFSRef _Nonnull _Locked self, DfsItem* _Nonnull item)
{
    const size_t idx = INID_HASH_INDEX(item->inid);

    List_InsertAfterLast(&self->inidChains[idx], &item->inidChain);
}

void DevFS_RemoveItem(DevFSRef _Nonnull _Locked self, InodeId inid)
{
    const size_t idx = INID_HASH_INDEX(inid);

    List_ForEach(&self->inidChains[idx], DfsItem,
        if (pCurNode->inid == inid) {
            List_Remove(&self->inidChains[idx], &pCurNode->inidChain);
            return;
        }
    )
}

DfsItem* _Nullable DevFS_GetItem(DevFSRef _Nonnull _Locked self, InodeId inid)
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

errno_t DevFS_createChannel(DevFSRef _Nonnull self, InodeRef _Consuming _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();

    switch (Inode_GetFileType(pNode)) {
        case kFileType_Directory:
            return DirectoryChannel_Create(pNode, pOutChannel);

        case kFileType_Device: {
            DfsDriverItem* dip = Inode_GetDfsDriverItem(pNode);

            if ((err = Driver_Open(dip->instance, mode, dip->arg, pOutChannel)) == EOK) {
                Inode_Relinquish(pNode);
            }
            return err;
        }

        default:
            *pOutChannel = NULL;
            return EPERM;
    }
}

errno_t DevFS_readFile(DevFSRef _Nonnull self, FileChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EPERM;
}

errno_t DevFS_writeFile(DevFSRef _Nonnull self, FileChannelRef _Nonnull _Locked pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EPERM;
}

errno_t DevFS_truncateFile(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pFile, FileOffset length)
{
    return EPERM;
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
    if (Inode_IsDirectory(target) && Inode_GetLinkCount(target) > 1 && DfsDirectoryItem_IsEmpty(Inode_GetDfsDirectoryItem(target))) {
        throw(EBUSY);
    }


    try(DevFS_unlinkCore(self, target, dir));

catch:
    SELock_Unlock(&self->seLock);
    return err;
}

errno_t DevFS_link(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pName, UserId uid, GroupId gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
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

errno_t DevFS_move(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pNewName, UserId uid, GroupId gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    return EPERM;
}

errno_t DevFS_rename(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, const PathComponent* _Nonnull pNewName, UserId uid, GroupId gid)
{
    return EPERM;
}


class_func_defs(DevFS, Filesystem,
override_func_def(deinit, DevFS, Object)
override_func_def(onReadNodeFromDisk, DevFS, Filesystem)
override_func_def(onWriteNodeToDisk, DevFS, Filesystem)
override_func_def(onRemoveNodeFromDisk, DevFS, Filesystem)
override_func_def(start, DevFS, Filesystem)
override_func_def(stop, DevFS, Filesystem)
override_func_def(isReadOnly, DevFS, Filesystem)
override_func_def(acquireRootDirectory, DevFS, Filesystem)
override_func_def(acquireNodeForName, DevFS, Filesystem)
override_func_def(getNameOfNode, DevFS, Filesystem)
override_func_def(createChannel, DevFS, Filesystem)
override_func_def(createNode, DevFS, Filesystem)
override_func_def(readFile, DevFS, Filesystem)
override_func_def(writeFile, DevFS, Filesystem)
override_func_def(truncateFile, DevFS, Filesystem)
override_func_def(readDirectory, DevFS, Filesystem)
override_func_def(unlink, DevFS, Filesystem)
override_func_def(move, DevFS, Filesystem)
override_func_def(rename, DevFS, Filesystem)
);
