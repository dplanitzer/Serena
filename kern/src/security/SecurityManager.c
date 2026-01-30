//
//  SecurityManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "SecurityManager.h"
#include <ext/perm.h>
#include <filesystem/Filesystem.h>
#include <kern/kalloc.h>
#include <kpi/signal.h>

typedef struct SecurityManager {
    int dummy;
} SecurityManager;


SecurityManagerRef _Nonnull  gSecurityManager;

errno_t SecurityManager_Create(SecurityManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    SecurityManager* self;
    
    try(kalloc_cleared(sizeof(SecurityManager), (void**) &self));

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

errno_t SecurityManager_CheckNodeAccess(SecurityManagerRef _Nonnull self, InodeRef _Nonnull _Locked pNode, uid_t uid, gid_t gid, int mode)
{
    const mode_t nodePerms = Inode_GetMode(pNode);
    mode_t reqPerms = 0;

    // XXX probably temporary until we're getting around to designing a full permission model
    if (uid == kUserId_Root) {
        return EOK;
    }
    // XXX
    
    if ((mode & R_OK) == R_OK) {
        reqPerms |= S_IREAD;
    }
    if ((mode & W_OK) == W_OK) {
        reqPerms |= S_IWRITE;

        // Return EROFS if write permissions are requested but the disk is read-only.
        if (Filesystem_IsReadOnly(Inode_GetFilesystem(pNode))) {
            return EROFS;
        }
    }
    if ((mode & X_OK) == X_OK) {
        reqPerms |= S_IEXEC;
    }


    mode_t finalPerms;

    if (Inode_GetUserId(pNode) == uid) {
        finalPerms = perm_get(nodePerms, S_ICUSR);
    }
    else if (Inode_GetGroupId(pNode) == gid) {
        finalPerms = perm_get(nodePerms, S_ICGRP);
    }
    else {
        finalPerms = perm_get(nodePerms, S_ICOTH);
    }


    if ((finalPerms & reqPerms) == reqPerms) {
        return EOK;
    }
    else {
        return EACCESS;
    }
}

errno_t SecurityManager_CheckNodeStatusUpdatePermission(SecurityManagerRef _Nonnull self, InodeRef _Nonnull _Locked pNode, uid_t uid)
{
    // XXX probably temporary until we're getting around to designing a full permission model
    if (uid == kUserId_Root) {
        return EOK;
    }
    // XXX
    
    // Return EROFS if disk is read-only.
    if (Filesystem_IsReadOnly(Inode_GetFilesystem(pNode))) {
        return EROFS;
    }

    return (Inode_GetUserId(pNode) == uid) ? EOK : EPERM;
}

bool SecurityManager_CanSendSignal(SecurityManagerRef _Nonnull self, const sigcred_t* _Nonnull sndr, const sigcred_t* _Nonnull rcv, int signo)
{
    if (sndr->uid == kUserId_Root) {
        return true;
    }
    if (sndr->uid == rcv->uid) {
        return true;
    }
    if (signo == SIGCHLD && sndr->ppid == rcv->pid) {
        return true;
    }

    return false;
}

bool SecurityManager_IsSuperuser(SecurityManagerRef _Nonnull self, uid_t uid)
{
    return uid == kUserId_Root;
}
