//
//  DevFS_Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DevFSPriv.h"
#include "DfsDirectory.h"
#include "DfsDevice.h"


// Returns a strong reference to the driver backing the given driver node.
// Returns NULL if the given node is not a driver node.
DriverRef _Nullable DevFS_CopyDriverForNode(DevFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    if (Inode_GetFileType(pNode) == kFileType_Device) {
        return Object_RetainAs(DfsDevice_GetItem(pNode)->instance, Driver);
    }
    else {
        return NULL;
    }
}

static errno_t _DevFS_createNode(DevFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable extra1, intptr_t extra2, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
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
            try(DfsDeviceItem_Create(DevFS_GetNextAvailableInodeId(self), permissions, uid, gid, (DriverRef)extra1, extra2, (DfsDeviceItem**)&pItem));
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
errno_t DevFS_CreateDevice(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, DriverRef _Nonnull pDriverInstance, intptr_t arg, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _DevFS_createNode(self, kFileType_Device, dir, name, pDriverInstance, arg, uid, gid, permissions, pOutNode);
}

errno_t DevFS_createNode(DevFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable dirInsertionHint, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _DevFS_createNode(self, type, dir, name, dirInsertionHint, 0, uid, gid, permissions, pOutNode);
}

errno_t DevFS_onAcquireNode(DevFSRef _Nonnull self, ino_t inid, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    DfsItem* ip = DevFS_GetItem(self, inid);

    if (ip) {
        switch (ip->type) {
            case kFileType_Device:
                return DfsDevice_Create(self, inid, (DfsDeviceItem*)ip, pOutNode);

            case kFileType_Directory:
                return DfsDirectory_Create(self, inid, (DfsDirectoryItem*)ip, pOutNode);

            default:
                break;
        }
    }

    *pOutNode = NULL;
    return EIO;
}

errno_t DevFS_onWritebackNode(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    decl_try_err();
    const ino_t id = Inode_GetId(pNode);
    const bool doDelete = (Inode_GetLinkCount(pNode) == 0) ? true : false;

    switch (Inode_GetFileType(pNode)) {
        case kFileType_Device:
            if (doDelete) {
                DevFS_RemoveItem(self, id);
                DfsItem_Destroy((DfsItem*)DfsDevice_GetItem(pNode));
            }
            else {
                DfsDevice_Serialize(pNode);
            }
            break;

        case kFileType_Directory:
            if (doDelete) {
                DevFS_RemoveItem(self, id);
                DfsItem_Destroy((DfsItem*)DfsDirectory_GetItem(pNode));
            }
            else {
                DfsDirectory_Serialize(pNode);
            }
            break;

        default:
            err = EIO;
            break;
    }

    return err;
}
