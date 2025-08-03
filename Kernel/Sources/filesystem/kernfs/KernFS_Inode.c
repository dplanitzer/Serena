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
#include "KfsProcess.h"


static errno_t _KernFS_createNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable extra1, intptr_t extra2, uid_t uid, gid_t gid, mode_t mode, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    KfsNodeRef ip = NULL;

    try(KfsDirectory_CanAcceptEntry((KfsDirectoryRef)dir, name, mode & S_IFMT));

    switch (mode & S_IFMT) {
        case S_IFDIR:
            try(KfsDirectory_Create(self, KernFS_GetNextAvailableInodeId(self), mode, uid, gid, Inode_GetId(dir), &ip));
            break;

        case S_IFDEV:
            try(KfsDevice_Create(self, KernFS_GetNextAvailableInodeId(self), mode, uid, gid, Inode_GetId(dir), (DriverRef)extra1, extra2, &ip));
            break;

        case S_IFFS:
            try(KfsFilesystem_Create(self, KernFS_GetNextAvailableInodeId(self), mode, uid, gid, Inode_GetId(dir), (FilesystemRef)extra1, &ip));
            break;

        case S_IFPROC:
            try(KfsProcess_Create(self, KernFS_GetNextAvailableInodeId(self), mode, uid, gid, Inode_GetId(dir), (ProcessRef)extra1, &ip));
            break;

        default:
            throw(EIO);
            break;
    }

    _KernFS_AddInode(self, ip);

    Inode_Lock(ip);
    err = KfsDirectory_InsertEntry((KfsDirectoryRef)dir, Inode_GetId(ip), S_ISDIR(Inode_GetMode(ip)), name);
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
errno_t KernFS_CreateDeviceNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, DriverRef _Nonnull dev, intptr_t arg, uid_t uid, gid_t gid, mode_t permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _KernFS_createNode(self, dir, name, dev, arg, uid, gid, __S_MKMODE(S_IFDEV, permissions), pOutNode);
}

// Creates a new filesystem node in the file system.
errno_t KernFS_CreateFilesystemNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, FilesystemRef _Nonnull fs, uid_t uid, gid_t gid, mode_t permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _KernFS_createNode(self, dir, name, fs, 0, uid, gid, __S_MKMODE(S_IFFS, permissions), pOutNode);
}

// Creates a new process node in the file system.
errno_t KernFS_CreateProcessNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, ProcessRef _Nonnull proc, uid_t uid, gid_t gid, mode_t permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _KernFS_createNode(self, dir, name, proc, 0, uid, gid, __S_MKMODE(S_IFPROC, permissions), pOutNode);
}

errno_t KernFS_createNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable dirInsertionHint, uid_t uid, gid_t gid, mode_t mode, InodeRef _Nullable * _Nonnull pOutNode)
{
    return _KernFS_createNode(self, dir, name, dirInsertionHint, 0, uid, gid, mode, pOutNode);
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
        struct timespec now;
        
        FSGetCurrentTime(&now);

        if (Inode_IsAccessed(pNode)) {
            Inode_SetAccessTime(pNode, &now);
        }
        if (Inode_IsUpdated(pNode)) {
            Inode_SetModificationTime(pNode, &now);
        }
        if (Inode_IsStatusChanged(pNode)) {
            Inode_SetStatusChangeTime(pNode, &now);
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
