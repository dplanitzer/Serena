//
//  KernFS_Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "KernFSPriv.h"
#include "KfsDirectory.h"
#include "KfsDevice.h"
#include "KfsFilesystem.h"


// Returns a strong reference to the driver backing the given driver node.
// Returns NULL if the given node is not a driver node.
DriverRef _Nullable KernFS_CopyDriverForNode(KernFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    if (Inode_GetFileType(pNode) == kFileType_Device) {
        return Object_RetainAs(((KfsDeviceRef)pNode)->instance, Driver);
    }
    else {
        return NULL;
    }
}

static errno_t _KernFS_createNode(KernFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable extra1, intptr_t extra2, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    KfsNodeRef ip = NULL;

    try(KfsDirectory_CanAcceptEntry((KfsDirectoryRef)dir, name, type));

    switch (type) {
        case kFileType_Directory:
            try(KfsDirectory_Create(self, KernFS_GetNextAvailableInodeId(self), permissions, uid, gid, Inode_GetId(dir), &ip));
            break;

        case kFileType_Device:
            try(KfsDevice_Create(self, KernFS_GetNextAvailableInodeId(self), permissions, uid, gid, Inode_GetId(dir), (DriverRef)extra1, extra2, &ip));
            break;

        case kFileType_Filesystem:
            try(KfsFilesystem_Create(self, KernFS_GetNextAvailableInodeId(self), permissions, uid, gid, Inode_GetId(dir), (FilesystemRef)extra1, &ip));
            break;

        default:
            throw(EIO);
            break;
    }

    _KernFS_AddInode(self, ip);

    Inode_Lock(ip);
    err = KfsDirectory_InsertEntry((KfsDirectoryRef)dir, Inode_GetId(ip), Inode_IsDirectory(ip), name);
    if (err == EOK) {
        Inode_Writeback(dir);
    }
    Inode_Unlock(ip);
    throw_iferr(err);
    
    try(Filesystem_AcquireNodeWithId((FilesystemRef)self, Inode_GetId(ip), pOutNode));

    return err;

catch:
    if (ip) {
        _KernFS_DestroyInode(self, ip);
    }

    *pOutNode = NULL;

    return err;
}

// Creates a new device node in the file system.
errno_t KernFS_CreateDevice(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, DriverRef _Nonnull pDriverInstance, intptr_t arg, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _KernFS_createNode(self, kFileType_Device, dir, name, pDriverInstance, arg, uid, gid, permissions, pOutNode);
}

// Creates a new filesystem node in the file system.
errno_t KernFS_CreateFilesystem(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, FilesystemRef _Nonnull fsInstance, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _KernFS_createNode(self, kFileType_Filesystem, dir, name, fsInstance, 0, uid, gid, permissions, pOutNode);
}

errno_t KernFS_createNode(KernFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable dirInsertionHint, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _KernFS_createNode(self, type, dir, name, dirInsertionHint, 0, uid, gid, permissions, pOutNode);
}

errno_t KernFS_onAcquireNode(KernFSRef _Nonnull self, ino_t inid, InodeRef _Nullable * _Nonnull pOutNode)
{
    // Caller is holding the seLock
    KfsNodeRef ip = _KernFS_GetInode(self, inid);

    *pOutNode = (InodeRef)ip;
    return (ip) ? EOK : ENODEV;
}

errno_t KernFS_onWritebackNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
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

void KernFS_onRelinquishNode(KernFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    // Caller is holding the seLock
    if (Inode_GetLinkCount(pNode) == 0) {
        _KernFS_DestroyInode(self, (KfsNodeRef)pNode);
    }
}
