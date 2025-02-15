//
//  Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Inode.h"
#include "Filesystem.h"
#include "FSUtilities.h"

errno_t Inode_Create(FilesystemRef _Nonnull pFS, InodeId id, FileType type, int linkCount, UserId uid, GroupId gid, FilePermissions permissions, FileOffset size, TimeInterval accessTime, TimeInterval modTime, TimeInterval statusChangeTime, void* refcon, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    InodeRef self;

    try(FSAllocateCleared(sizeof(Inode), (void**) &self));
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
        FSDeallocate(self);
    }
}

void Inode_Unlink(InodeRef _Nonnull self)
{
    if (self->linkCount > 0) {
        self->linkCount--;
    }
}

// Returns true if the receiver and 'pOther' are the same node; false otherwise
bool Inode_Equals(InodeRef _Nonnull self, InodeRef _Nonnull pOther)
{
    return self->inid == pOther->inid
        && Filesystem_GetId(self->filesystem) == Filesystem_GetId(pOther->filesystem);
}

errno_t Inode_Relinquish(InodeRef _Nullable self)
{
    decl_try_err();

    if (self) {
        err = Filesystem_RelinquishNode(self->filesystem, self);
    }
    return err;
}

errno_t Inode_UnlockRelinquish(InodeRef _Nullable _Locked self)
{
    decl_try_err();

    if (self) {
        Inode_Unlock(self);
        err = Filesystem_RelinquishNode(self->filesystem, self);
    }
    return err;
}
