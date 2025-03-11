//
//  SerenaFS_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"


errno_t SerenaFS_acquireNodeForName(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull pName, uid_t uid, gid_t gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    decl_try_err();
    sfs_query_t q;
    sfs_query_result_t qr;

    q.kind = kSFSQuery_PathComponent;
    q.u.pc = pName;
    q.mpc = NULL;
    q.ih = (sfs_insertion_hint_t*)pDirInsHint;

    err = SfsDirectory_Query(dir, &q, &qr);
    if (err == EOK && pOutNode) {
        err = Filesystem_AcquireNodeWithId((FilesystemRef)self, qr.id, pOutNode);
    }
    if (err != EOK && pOutNode) {
        *pOutNode = NULL;
    }
    return err;
}

errno_t SerenaFS_getNameOfNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, ino_t id, uid_t uid, gid_t gid, MutablePathComponent* _Nonnull pName)
{
    sfs_query_t q;
    sfs_query_result_t qr;

    q.kind = kSFSQuery_InodeId;
    q.u.id = id;
    q.mpc = pName;
    q.ih = NULL;
    
    return SfsDirectory_Query(dir, &q, &qr);
}
