//
//  Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Inode.h"
#include "FilesystemManager.h"
#include <driver/MonotonicClock.h>

errno_t Inode_Create(FilesystemRef _Nonnull pFS, InodeId id, FileType type, int linkCount, UserId uid, GroupId gid, FilePermissions permissions, FileOffset size, TimeInterval accessTime, TimeInterval modTime, TimeInterval statusChangeTime, void* refcon, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    InodeRef self;

    try(kalloc_cleared(sizeof(Inode), (void**) &self));
    self->accessTime = accessTime;
    self->modificationTime = modTime;
    self->statusChangeTime = statusChangeTime;
    self->size = size;
    Lock_Init(&self->lock);
    self->filesystem = pFS;
    self->inid = id;
    self->useCount = 0;
    self->linkCount = linkCount;
    self->refcon = refcon;
    self->type = type;
    self->flags = 0;
    self->permissions = permissions;
    self->uid = uid;
    self->gid = gid;

    *pOutNode = self;
    return EOK;

catch:
    *pOutNode = NULL;
    return err;
}

void Inode_Destroy(InodeRef _Nullable self)
{
    if (self) {
        self->filesystem = NULL;
        self->refcon = NULL;
        Lock_Deinit(&self->lock);
        kfree(self);
    }
}

void Inode_Unlink(InodeRef _Nonnull self)
{
    if (self->linkCount > 0) {
        self->linkCount--;
    }
}

// Returns EOK if the given user has at least the permissions 'permission' to
// access and/or manipulate the node; a suitable error code otherwise. The
// 'permission' parameter represents a set of the permissions of a single
// permission scope.
errno_t Inode_CheckAccess(InodeRef _Nonnull self, User user, FilePermissions permission)
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
    TimeInterval curTime;

    if (Inode_IsModified(self)) {
        curTime = MonotonicClock_GetCurrentTime();
    }

    if (Inode_IsAccessed(self)) {
        pOutInfo->accessTime = curTime;
    } else {
        pOutInfo->accessTime = self->accessTime;
    }
    if (Inode_IsUpdated(self)) {
        pOutInfo->modificationTime = curTime;
    } else {
        pOutInfo->modificationTime = self->modificationTime;
    }
    if (Inode_IsStatusChanged(self)) {
        pOutInfo->statusChangeTime = curTime;
    } else {
        pOutInfo->statusChangeTime = self->statusChangeTime;
    }
    
    pOutInfo->size = self->size;
    pOutInfo->uid = self->uid;
    pOutInfo->gid = self->gid;
    pOutInfo->permissions = self->permissions;
    pOutInfo->type = self->type;
    pOutInfo->reserved = 0;
    pOutInfo->linkCount = self->linkCount;
    pOutInfo->filesystemId = Filesystem_GetId(self->filesystem);
    pOutInfo->inodeId = self->inid;
}

// Modifies the node's file info if the operation is permissible based on the
// given user and inode permissions status.
errno_t Inode_SetFileInfo(InodeRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo)
{
    uint32_t  modify = pInfo->modify;

    // We first validate that the request operation is permissible.
    if ((modify & (kModifyFileInfo_UserId|kModifyFileInfo_GroupId)) != 0) {
        if (user.uid != Inode_GetUserId(self) && user.uid != kRootUserId) {
            return EPERM;
        }
    }


    // We got permissions. Now update the data as requested.
    if ((modify & kModifyFileInfo_UserId) == kModifyFileInfo_UserId) {
        self->uid = pInfo->uid;
    }

    if ((modify & kModifyFileInfo_GroupId) == kModifyFileInfo_GroupId) {
        self->gid = pInfo->gid;
    }

    if ((modify & kModifyFileInfo_Permissions) == kModifyFileInfo_Permissions) {
        self->permissions &= ~pInfo->permissionsModifyMask;
        self->permissions |= (pInfo->permissions & pInfo->permissionsModifyMask);
    }

    // Update timestamps
    if ((modify & kModifyFileInfo_AccessTime) == kModifyFileInfo_AccessTime) {
        self->accessTime = pInfo->accessTime;
        Inode_SetModified(self, kInodeFlag_Accessed);
    }
    if ((modify & kModifyFileInfo_ModificationTime) == kModifyFileInfo_ModificationTime) {
        self->modificationTime = pInfo->modificationTime;
        Inode_SetModified(self, kInodeFlag_Updated);
    }

    Inode_SetModified(self, kInodeFlag_StatusChanged);

    return EOK;
}

// Returns true if the receiver and 'pOther' are the same node; false otherwise
bool Inode_Equals(InodeRef _Nonnull self, InodeRef _Nonnull pOther)
{
    return self->inid == pOther->inid
        && Filesystem_GetId(self->filesystem) == Filesystem_GetId(pOther->filesystem);
}
