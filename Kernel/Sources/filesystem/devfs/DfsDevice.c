//
//  DfsDevice.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DfsDevice.h"
#include <driver/Driver.h>
#include <filesystem/FSUtilities.h>
#include <kobj/AnyRefs.h>


errno_t DfsDevice_Create(DevFSRef _Nonnull fs, InodeId inid, DfsDeviceItem* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    DfsDeviceRef self;

    err = Inode_Create(
        class(DfsDevice),
        (FilesystemRef)fs,
        inid,
        kFileType_Device,
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

void DfsDevice_Serialize(InodeRef _Nonnull _Locked self)
{
    DfsDeviceItem* _Nonnull ip = DfsDevice_GetItem(self);
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


class_func_defs(DfsDevice, Inode,
//override_func_def(onStart, ZRamDriver, Driver)
);
