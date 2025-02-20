//
//  DfsDirectory.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DfsDirectory.h"
#include <driver/Driver.h>
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FSUtilities.h>
#include <kobj/AnyRefs.h>


errno_t DfsDirectory_Create(DevFSRef _Nonnull fs, InodeId inid, DfsDirectoryItem* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    DfsDirectoryRef self;

    err = Inode_Create(
        class(DfsDirectory),
        (FilesystemRef)fs,
        inid,
        kFileType_Directory,
        ip->super.linkCount,
        ip->super.uid,
        ip->super.gid,
        ip->super.permissions,
        ip->super.size,
        ip->super.accessTime,
        ip->super.modificationTime,
        ip->super.statusChangeTime,
        (InodeRef*)&self);
    if (err == EOK) {
        self->item = ip;
    }
    *pOutNode = (InodeRef)self;
    return err;
}

void DfsDirectory_Serialize(InodeRef _Nonnull _Locked self)
{
    DfsDirectoryItem* _Nonnull ip = DfsDirectory_GetItem(self);
    const TimeInterval curTime = FSGetCurrentTime();

    ip->super.accessTime = (Inode_IsAccessed(self)) ? curTime : Inode_GetAccessTime(self);
    ip->super.modificationTime = (Inode_IsUpdated(self)) ? curTime : Inode_GetModificationTime(self);
    ip->super.statusChangeTime = (Inode_IsStatusChanged(self)) ? curTime : Inode_GetStatusChangeTime(self);
    ip->super.size = Inode_GetFileSize(self);
    ip->super.uid = Inode_GetUserId(self);
    ip->super.gid = Inode_GetGroupId(self);
    ip->super.linkCount = Inode_GetLinkCount(self);
    ip->super.permissions = Inode_GetFilePermissions(self);
}

errno_t DfsDirectory_createChannel(InodeRef _Nonnull self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return DirectoryChannel_Create(self, pOutChannel);
}


class_func_defs(DfsDirectory, Inode,
override_func_def(createChannel, DfsDirectory, Inode)
);
