//
//  perm_check.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "perm_check.h"
#include <filesystem/Filesystem.h>
#include <filesystem/Inode.h>
#include <kern/kalloc.h>
#include <kpi/fs_perms.h>
#include <kpi/signal.h>

errno_t perm_check_node_access(InodeRef _Nonnull _Locked pNode, uid_t uid, gid_t gid, int mode)
{
    const fs_perms_t fsperms = Inode_GetPermissions(pNode);
    fs_perms_t reqPerms = 0;

    // XXX probably temporary until we're getting around to designing a full permission model
    if (uid == UID_ROOT) {
        return EOK;
    }
    // XXX
    
    if ((mode & R_OK) == R_OK) {
        reqPerms |= FS_PRM_R;
    }
    if ((mode & W_OK) == W_OK) {
        reqPerms |= FS_PRM_W;

        // Return EROFS if write permissions are requested but the disk is read-only.
        if (Filesystem_IsReadOnly(Inode_GetFilesystem(pNode))) {
            return EROFS;
        }
    }
    if ((mode & X_OK) == X_OK) {
        reqPerms |= FS_PRM_X;
    }


    fs_perms_t finalPerms;

    if (Inode_GetUserId(pNode) == uid) {
        finalPerms = fs_perms_get(fsperms, FS_CLS_USR);
    }
    else if (Inode_GetGroupId(pNode) == gid) {
        finalPerms = fs_perms_get(fsperms, FS_CLS_GRP);
    }
    else {
        finalPerms = fs_perms_get(fsperms, FS_CLS_OTH);
    }


    if ((finalPerms & reqPerms) == reqPerms) {
        return EOK;
    }
    else {
        return EACCESS;
    }
}

errno_t perm_check_node_attr_update(InodeRef _Nonnull _Locked pNode, uid_t uid)
{
    // XXX probably temporary until we're getting around to designing a full permission model
    if (uid == UID_ROOT) {
        return EOK;
    }
    // XXX
    
    // Return EROFS if disk is read-only.
    if (Filesystem_IsReadOnly(Inode_GetFilesystem(pNode))) {
        return EROFS;
    }

    return (Inode_GetUserId(pNode) == uid) ? EOK : EPERM;
}

errno_t perm_check_send_signal(const sigcred_t* _Nonnull sndr, const sigcred_t* _Nonnull rcv, int signo)
{
    if (sndr->uid == UID_ROOT) {
        return EOK;
    }
    if (sndr->uid == rcv->uid) {
        return EOK;
    }
    if (signo == SIG_CHILD && sndr->ppid == rcv->pid) {
        return EOK;
    }

    return EPERM;
}
