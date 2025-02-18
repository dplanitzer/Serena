//
//  DevFS_Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DevFSPriv.h"
#include "DfsNode.h"


// Returns a strong reference to the driver backing the given driver node.
// Returns NULL if the given node is not a driver node.
DriverRef _Nullable DevFS_CopyDriverForNode(DevFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    return DfsNode_CopyDriver(pNode);
}

static errno_t _DevFS_createNode(DevFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable extra1, intptr_t extra2, UserId uid, GroupId gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    const TimeInterval curTime = FSGetCurrentTime();
    DfsItem* pItem = NULL;

    try_bang(SELock_LockExclusive(&self->seLock));

    switch (type) {
        case kFileType_Directory:
            // Make sure that the parent directory is able to accept one more link
            if (Inode_GetLinkCount(dir) >= MAX_LINK_COUNT) {
                throw(EMLINK);
            }

            try(DfsDirectoryItem_Create(DevFS_GetNextAvailableInodeId(self), permissions, uid, gid, Inode_GetId(dir), (DfsDirectoryItem**)&pItem));
            break;

        case kFileType_Device:
            try(DfsDriverItem_Create(DevFS_GetNextAvailableInodeId(self), permissions, uid, gid, (DriverRef)extra1, extra2, (DfsDriverItem**)&pItem));
            break;

        default:
            throw(EIO);
            break;
    }

    DevFS_AddItem(self, pItem);

    try(DevFS_InsertDirectoryEntry(self, dir, pItem->inid, name));
    if (type == kFileType_Directory) {
        // Increment the parent directory link count to account for the '..' entry
        // in the just created subdirectory
        Inode_Link(dir);
    }

    try(Filesystem_AcquireNodeWithId((FilesystemRef)self, pItem->inid, pOutNode));

    SELock_Unlock(&self->seLock);
    return err;

catch:
    if (pItem) {
        DevFS_RemoveItem(self, pItem->inid);
        DfsItem_Destroy(pItem);
    }

    *pOutNode = NULL;
    SELock_Unlock(&self->seLock);

    return err;
}

// Creates a new device node in the file system.
errno_t DevFS_CreateDevice(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, DriverRef _Nonnull pDriverInstance, intptr_t arg, UserId uid, GroupId gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _DevFS_createNode(self, kFileType_Device, dir, name, pDriverInstance, arg, uid, gid, permissions, pOutNode);
}

errno_t DevFS_createNode(DevFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable dirInsertionHint, UserId uid, GroupId gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _DevFS_createNode(self, type, dir, name, dirInsertionHint, 0, uid, gid, permissions, pOutNode);
}

errno_t DevFS_onReadNodeFromDisk(DevFSRef _Nonnull self, InodeId inid, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    DfsItem* ip = DevFS_GetItem(self, inid);

    if (ip) {
        return DfsNode_Create(self, inid, ip, pOutNode);
    }
    else {
        *pOutNode = NULL;
        return EIO;
    }
}

errno_t DevFS_onWriteNodeToDisk(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    DfsNode_Serialize(pNode, DfsNode_GetItem(pNode));
    return EOK;
}

void DevFS_onRemoveNodeFromDisk(DevFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    DevFS_RemoveItem(self, Inode_GetId(pNode));
    DfsItem_Destroy(DfsNode_GetItem(pNode));
}
