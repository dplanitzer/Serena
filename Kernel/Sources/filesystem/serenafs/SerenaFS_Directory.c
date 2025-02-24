//
//  SerenaFS_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"


errno_t SerenaFS_acquireRootDirectory(SerenaFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();

    err = SELock_LockShared(&self->seLock);
    if (err == EOK) {
        if (self->mountFlags.isMounted) {
            err = Filesystem_AcquireNodeWithId((FilesystemRef)self, self->rootDirLba, pOutDir);
        }
        else {
            err = EIO;
        }
        SELock_Unlock(&self->seLock);
    }
    return err;
}

errno_t SerenaFS_acquireNodeForName(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, UserId uid, GroupId gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    decl_try_err();
    SFSDirectoryQuery q;
    InodeId entryId;

    q.kind = kSFSDirectoryQuery_PathComponent;
    q.u.pc = pName;
    err = SfsDirectory_GetEntry(pDir, &q, (pDirInsHint) ? (sfs_dirent_ptr_t*)pDirInsHint->data : NULL, NULL, &entryId, NULL);
    if (err == EOK && pOutNode) {
        err = Filesystem_AcquireNodeWithId((FilesystemRef)self, entryId, pOutNode);
    }
    if (err != EOK && pOutNode) {
        *pOutNode = NULL;
    }
    return err;
}

errno_t SerenaFS_getNameOfNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId id, UserId uid, GroupId gid, MutablePathComponent* _Nonnull pName)
{
    SFSDirectoryQuery q;

    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = id;
    return SfsDirectory_GetEntry(pDir, &q, NULL, NULL, NULL, pName);
}
