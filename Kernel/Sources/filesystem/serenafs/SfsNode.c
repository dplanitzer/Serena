//
//  SfsNode.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "SfsNode.h"
#include <filesystem/FSUtilities.h>
#include <kobj/AnyRefs.h>
#include <System/ByteOrder.h>


errno_t SfsNode_Create(SerenaFSRef _Nonnull fs, InodeId inid, const SFSInode* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    SfsNodeRef self;

    err = Inode_Create(
        class(SfsNode),
        (FilesystemRef)fs,
        inid,
        ip->type,
        Int32_BigToHost(ip->linkCount),
        UInt32_BigToHost(ip->uid),
        UInt32_BigToHost(ip->gid),
        UInt16_BigToHost(ip->permissions),
        Int64_BigToHost(ip->size),
        TimeInterval_Make(UInt32_BigToHost(ip->accessTime.tv_sec), UInt32_BigToHost(ip->accessTime.tv_nsec)),
        TimeInterval_Make(UInt32_BigToHost(ip->modificationTime.tv_sec), UInt32_BigToHost(ip->modificationTime.tv_nsec)),
        TimeInterval_Make(UInt32_BigToHost(ip->statusChangeTime.tv_sec), UInt32_BigToHost(ip->statusChangeTime.tv_nsec)),
        (InodeRef*)&self);
    if (err == EOK) {
        memcpy(self->direct, ip->bp, sizeof(SFSBlockNumber) * kSFSInodeBlockPointersCount);
    }
    *pOutNode = (InodeRef)self;

    return err;
}

void SfsNode_Serialize(InodeRef _Nonnull _Locked pNode, SFSInode* _Nonnull ip)
{
    SfsNodeRef self = (SfsNodeRef)pNode;
    const TimeInterval curTime = FSGetCurrentTime();
    const TimeInterval accTime = (Inode_IsAccessed(self)) ? curTime : Inode_GetAccessTime(self);
    const TimeInterval modTime = (Inode_IsUpdated(self)) ? curTime : Inode_GetModificationTime(self);
    const TimeInterval chgTime = (Inode_IsStatusChanged(self)) ? curTime : Inode_GetStatusChangeTime(self);

    ip->accessTime.tv_sec = UInt32_HostToBig(accTime.tv_sec);
    ip->accessTime.tv_nsec = UInt32_HostToBig(accTime.tv_nsec);
    ip->modificationTime.tv_sec = UInt32_HostToBig(modTime.tv_sec);
    ip->modificationTime.tv_nsec = UInt32_HostToBig(modTime.tv_nsec);
    ip->statusChangeTime.tv_sec = UInt32_HostToBig(chgTime.tv_sec);
    ip->statusChangeTime.tv_nsec = UInt32_HostToBig(chgTime.tv_nsec);
    ip->size = Int64_HostToBig(Inode_GetFileSize(self));
    ip->uid = UInt32_HostToBig(Inode_GetUserId(self));
    ip->gid = UInt32_HostToBig(Inode_GetGroupId(self));
    ip->linkCount = Int32_HostToBig(Inode_GetLinkCount(self));
    ip->permissions = UInt16_HostToBig(Inode_GetFilePermissions(self));
    ip->type = Inode_GetFileType(self);

    memcpy(ip->bp, SfsNode_GetBlockMap(self), sizeof(uint32_t) * kSFSInodeBlockPointersCount);
}


class_func_defs(SfsNode, Inode,
//override_func_def(onStart, ZRamDriver, Driver)
);
