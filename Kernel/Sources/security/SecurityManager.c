//
//  SecurityManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "SecurityManager.h"
#include <filesystem/Filesystem.h>
#include <kern/kalloc.h>

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
    const FilePermissions nodePerms = Inode_GetFilePermissions(pNode);
    FilePermissions reqPerms = 0;

    // XXX probably temporary until we're getting around to designing a full permission model
    if (uid == kUserId_Root) {
        return EOK;
    }
    // XXX
    
    if ((mode & R_OK) == R_OK) {
        reqPerms |= kFilePermission_Read;
    }
    if ((mode & W_OK) == W_OK) {
        reqPerms |= kFilePermission_Write;

        // Return EROFS if write permissions are requested but the disk is read-only.
        if (Filesystem_IsReadOnly(Inode_GetFilesystem(pNode))) {
            return EROFS;
        }
    }
    if ((mode & X_OK) == X_OK) {
        reqPerms |= kFilePermission_Execute;
    }


    FilePermissions finalPerms;

    if (Inode_GetUserId(pNode) == uid) {
        finalPerms = FilePermissions_Get(nodePerms, kFilePermissionsClass_User);
    }
    else if (Inode_GetGroupId(pNode) == gid) {
        finalPerms = FilePermissions_Get(nodePerms, kFilePermissionsClass_Group);
    }
    else {
        finalPerms = FilePermissions_Get(nodePerms, kFilePermissionsClass_Other);
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
    const FilePermissions nodePerms = Inode_GetFilePermissions(pNode);
    FilePermissions reqPerms = 0;

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

bool SecurityManager_IsSuperuser(SecurityManagerRef _Nonnull self, uid_t uid)
{
    return uid == kUserId_Root;
}
