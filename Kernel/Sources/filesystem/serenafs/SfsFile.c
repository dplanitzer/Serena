//
//  SfsFile.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "SfsFile.h"
#include "SerenaFSPriv.h"
#include <filesystem/FileChannel.h>
#include <filesystem/FSUtilities.h>
#include <kobj/AnyRefs.h>
#include <System/ByteOrder.h>


errno_t SfsFile_Create(Class* _Nonnull pClass, SerenaFSRef _Nonnull fs, InodeId inid, const sfs_inode_t* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    SfsFileRef self;

    err = Inode_Create(
        pClass,
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
        memcpy(self->direct, ip->bp, sizeof(sfs_bno_t) * kSFSInodeBlockPointersCount);
    }
    *pOutNode = (InodeRef)self;

    return err;
}

void SfsFile_Serialize(InodeRef _Nonnull _Locked pNode, sfs_inode_t* _Nonnull ip)
{
    SfsFileRef self = (SfsFileRef)pNode;
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

    memcpy(ip->bp, SfsFile_GetBlockMap(self), sizeof(uint32_t) * kSFSInodeBlockPointersCount);
}

errno_t SfsFile_read(SfsFileRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked ch, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    SerenaFSRef fs = (SerenaFSRef)Inode_GetFilesystem(self);

    err = SerenaFS_xRead(fs, (InodeRef)self, IOChannel_GetOffset(ch), pBuffer, nBytesToRead, nOutBytesRead);
    IOChannel_IncrementOffsetBy(ch, *nOutBytesRead);

    return err;
}

errno_t SfsFile_write(SfsFileRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked ch, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    SerenaFSRef fs = (SerenaFSRef)Inode_GetFilesystem(self);
    FileOffset offset;

    if ((IOChannel_GetMode(ch) & kOpen_Append) == kOpen_Append) {
        offset = Inode_GetFileSize(self);
    }
    else {
        offset = IOChannel_GetOffset(ch);
    }

    err = SerenaFS_xWrite(fs, (InodeRef)self, offset, pBuffer, nBytesToWrite, nOutBytesWritten);
    IOChannel_IncrementOffsetBy(ch, *nOutBytesWritten);

    return err;
}

errno_t SfsFile_truncate(InodeRef _Nonnull _Locked self, FileOffset length)
{
    decl_try_err();
    SerenaFSRef fs = (SerenaFSRef)Inode_GetFilesystem(self);

    const FileOffset oldLength = Inode_GetFileSize(self);
    if (oldLength < length) {
        // Expansion in size
        // Just set the new file size. The needed blocks will be allocated on
        // demand when read/write is called to manipulate the new data range.
        Inode_SetFileSize(self, length);
        Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged); 
    }
    else if (oldLength > length) {
        // Reduction in size
        SerenaFS_xTruncateFile(fs, (InodeRef)self, length);
    }

    return err;
}


class_func_defs(SfsFile, Inode,
override_func_def(read, SfsFile, Inode)
override_func_def(write, SfsFile, Inode)
override_func_def(truncate, SfsFile, Inode)    
);
