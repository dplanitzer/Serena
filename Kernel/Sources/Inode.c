//
//  Inode.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Inode.h"
#include "FilesystemManager.h"

ErrorCode Inode_AbstractCreate(ClassRef _Nonnull pClass, FileType type, InodeId id, FilesystemId fsid, FilePermissions permissions, User user, FileOffset size, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    InodeRef pNode;

    try(_Object_Create(pClass, 0, (ObjectRef*)&pNode));
    pNode->type = type;
    pNode->permissions = permissions;
    pNode->user = user;
    pNode->inid = id;
    pNode->fsid = fsid;
    pNode->size = size;

    *pOutNode = pNode;
    return EOK;

catch:
    *pOutNode = NULL;
    return err;
}

// Returns a strong reference to the filesystem that owns the given note. Returns
// NULL if the filesystem isn't mounted.
FilesystemRef Inode_CopyFilesystem(InodeRef _Nonnull self)
{
    return FilesystemManager_CopyFilesystemForId(gFilesystemManager, Inode_GetFilesystemId(self));
}

// Returns EOK if the given user has at least the permissions 'permission' to
// access and/or manipulate the node; a suitable error code otherwise. The
// 'permission' parameter represents a set of the permissions of a single
// permission scope.
ErrorCode Inode_CheckAccess(InodeRef _Nonnull self, User user, FilePermissions permission)
{
    // XXX revisit this once we put a proper user permission model in place
    // XXX forcing superuser off for now since it's the only user we got at this
    // XXX time and we want the file permissions to actually do something.
    //if (user.uid == kRootUserId) {
    //    return EOK;
    //}

    FilePermissions reqPerms = 0;

    if (Inode_GetUserId(self) == user.uid) {
        reqPerms = FilePermissions_Make(permission, 0, 0);
    }
    else if (Inode_GetGroupId(self) == user.gid) {
        reqPerms = FilePermissions_Make(0, permission, 0);
    }
    else {
        reqPerms = FilePermissions_Make(0, 0, permission);
    }

    if ((Inode_GetFilePermissions(self) & reqPerms) == reqPerms) {
        return EOK;
    }

    return EACCESS;
}

// Returns a file info record from the node data.
void Inode_GetFileInfo(InodeRef _Nonnull self, FileInfo* _Nonnull pOutInfo)
{
    pOutInfo->lastAccessTime.seconds = 0;
    pOutInfo->lastAccessTime.nanoseconds = 0;
    pOutInfo->lastModificationTime.seconds = 0;
    pOutInfo->lastModificationTime.nanoseconds = 0;
    pOutInfo->lastStatusChangeTime.seconds = 0;
    pOutInfo->lastStatusChangeTime.nanoseconds = 0;
    pOutInfo->size = self->size;
    pOutInfo->uid = self->user.uid;
    pOutInfo->gid = self->user.gid;
    pOutInfo->permissions = self->permissions;
    pOutInfo->type = self->type;
    pOutInfo->reserved = 0;
    pOutInfo->filesystemId = self->fsid;
    pOutInfo->fileId = self->inid;
}

// Returns true if the receiver and 'pOther' are the same node; false otherwise
Bool Inode_Equals(InodeRef _Nonnull self, InodeRef _Nonnull pOther)
{
    return self->fsid == pOther->fsid && self->inid == pOther->inid;
}

void Inode_deinit(InodeRef _Nonnull self)
{
}

CLASS_METHODS(Inode, Object,
OVERRIDE_METHOD_IMPL(deinit, Inode, Object)
);
