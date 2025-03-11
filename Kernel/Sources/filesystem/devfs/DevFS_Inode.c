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
        return Object_RetainAs(((DfsDeviceRef)pNode)->instance, Driver);
    }
    else {
        return NULL;
    }
}

static errno_t _DevFS_createNode(DevFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable extra1, intptr_t extra2, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    DfsNodeRef ip = NULL;

    try(DfsDirectory_CanAcceptEntry((DfsDirectoryRef)dir, name, type));

    switch (type) {
        case kFileType_Directory:
            try(DfsDirectory_Create(self, DevFS_GetNextAvailableInodeId(self), permissions, uid, gid, Inode_GetId(dir), &ip));
            break;

        case kFileType_Device:
            try(DfsDevice_Create(self, DevFS_GetNextAvailableInodeId(self), permissions, uid, gid, (DriverRef)extra1, extra2, &ip));
            break;

        default:
            throw(EIO);
            break;
    }

    _DevFS_AddInode(self, ip);

    Inode_Lock(ip);
    err = DfsDirectory_InsertEntry((DfsDirectoryRef)dir, Inode_GetId(ip), Inode_IsDirectory(ip), name);
    if (err == EOK) {
        Inode_Writeback(dir);
    }
    Inode_Unlock(ip);
    throw_iferr(err);
    
    try(Filesystem_AcquireNodeWithId((FilesystemRef)self, Inode_GetId(ip), pOutNode));

    return err;

catch:
    if (ip) {
        _DevFS_DestroyInode(self, ip);
    }

    *pOutNode = NULL;

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
    // Caller is holding the seLock
    DfsNodeRef ip = _DevFS_GetInode(self, inid);

    *pOutNode = (InodeRef)ip;
    return (ip) ? EOK : ENODEV;
}

errno_t DevFS_onWritebackNode(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    if (Inode_IsModified(pNode)) {
        const TimeInterval curTime = FSGetCurrentTime();

        if (Inode_IsAccessed(pNode)) {
            Inode_SetAccessTime(pNode, curTime);
        }
        if (Inode_IsUpdated(pNode)) {
            Inode_SetModificationTime(pNode, curTime);
        }
        if (Inode_IsStatusChanged(pNode)) {
            Inode_SetStatusChangeTime(pNode, curTime);
        }
    }

    return EOK;
}

void DevFS_onRelinquishNode(DevFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    // Caller is holding the seLock
    if (Inode_GetLinkCount(pNode) == 0) {
        _DevFS_DestroyInode(self, (DfsNodeRef)pNode);
    }
}
