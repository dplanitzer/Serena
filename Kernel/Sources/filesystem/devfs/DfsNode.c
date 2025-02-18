//
//  DfsNode.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DfsNode.h"
#include <driver/Driver.h>
#include <filesystem/FSUtilities.h>
#include <kobj/AnyRefs.h>


errno_t DfsNode_Create(DevFSRef _Nonnull fs, InodeId inid, DfsItem* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    DfsNodeRef self;

    err = Inode_Create(
        class(DfsNode),
        (FilesystemRef)fs,
        inid,
        ip->type,
        ip->linkCount,
        ip->uid,
        ip->gid,
        ip->permissions,
        ip->size,
        ip->accessTime,
        ip->modificationTime,
        ip->statusChangeTime,
        (InodeRef*)&self);
    if (err == EOK) {
        self->u.item = ip;
    }
    *pOutNode = (InodeRef)self;
    return err;
}

void DfsNode_Serialize(InodeRef _Nonnull _Locked pNode, DfsItem* _Nonnull ip)
{
    DfsNodeRef self = (DfsNodeRef)pNode;
    const TimeInterval curTime = FSGetCurrentTime();

    ip->accessTime = (Inode_IsAccessed(self)) ? curTime : Inode_GetAccessTime(self);
    ip->modificationTime = (Inode_IsUpdated(self)) ? curTime : Inode_GetModificationTime(self);
    ip->statusChangeTime = (Inode_IsStatusChanged(self)) ? curTime : Inode_GetStatusChangeTime(self);
    ip->size = Inode_GetFileSize(self);
    ip->uid = Inode_GetUserId(self);
    ip->gid = Inode_GetGroupId(self);
    ip->linkCount = Inode_GetLinkCount(self);
    ip->permissions = Inode_GetFilePermissions(self);
    ip->type = Inode_GetFileType(self);
}

// Returns a strong reference to the driver backing the given driver node.
// Returns NULL if the given node is not a driver node.
DriverRef _Nullable DfsNode_CopyDriver(InodeRef _Nonnull pNode)
{
    if (Inode_GetFileType(pNode) == kFileType_Device) {
        return Object_RetainAs(DfsNode_GetDriver(pNode)->instance, Driver);
    }
    else {
        return NULL;
    }
}


class_func_defs(DfsNode, Inode,
//override_func_def(onStart, ZRamDriver, Driver)
);
