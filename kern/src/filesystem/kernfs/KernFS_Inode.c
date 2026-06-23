//
//  KernFS_Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "KernFSPriv.h"
#include "KfsDirectory.h"
#include "KfsSpecial.h"
#include <driver/Driver.h>
#include <kpi/attr.h>


static errno_t _KernFS_createNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, KfsNodeRef _Nonnull ip, fs_ftype_t ftype, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();

    try(KfsDirectory_CanAcceptEntry((KfsDirectoryRef)dir, name, ftype));

    _KernFS_AddInode(self, ip);

    Inode_Lock(ip);
    err = KfsDirectory_InsertEntry((KfsDirectoryRef)dir, Inode_GetId(ip), Inode_IsDirectory(ip), name);
    if (err == EOK) {
        Inode_Writeback(dir);
    }
    Inode_Unlock(ip);
    throw_iferr(err);
    
    try(Filesystem_AcquireNodeWithId((FilesystemRef)self, Inode_GetId(ip), pOutNode));

catch:
    return err;
}

// Creates a new node that has no backing resource and will invoke the provided handler factory function when needed.
errno_t KernFS_CreateHandlerNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, const KfsHandlerNodeArgs* _Nonnull args, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    KfsNodeRef ip = NULL;

    err = KfsSpecial_Create(self, KernFS_GetNextAvailableInodeId(self), Inode_GetId(dir), args, &ip);
    if (err == EOK) {
        err = _KernFS_createNode(self, dir, name, ip, FS_FTYPE_DEV, pOutNode);

        if (err != EOK) {
            _KernFS_DestroyInode(self, ip);
        }
    }

    return err;
}

errno_t KernFS_createNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable dirInsertionHint, uid_t uid, gid_t gid, fs_ftype_t ftype, fs_perms_t fsperms, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    KfsNodeRef ip = NULL;

    switch (ftype) {
        case FS_FTYPE_DIR:
            err = KfsDirectory_Create(self, KernFS_GetNextAvailableInodeId(self), fsperms, uid, gid, Inode_GetId(dir), &ip);
            break;

        default:
            err = EPERM;
            break;
    }

    if (err == EOK) {
        err = _KernFS_createNode(self, dir, name, ip, ftype, pOutNode);

        if (err != EOK) {
            _KernFS_DestroyInode(self, ip);
        }
    }

    return err;
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
        nanotime_t now;
        
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
