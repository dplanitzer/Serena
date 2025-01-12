//
//  SerenaFS_Format.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <System/ByteOrder.h>


// Formats the given disk drive and installs a SerenaFS with an empty root
// directory on it. 'user' and 'permissions' are the user and permissions that
// should be assigned to the root directory.
errno_t SerenaFS_FormatDrive(FSContainerRef _Nonnull fsContainer, UserId uid, GroupId gid, FilePermissions permissions)
{
    decl_try_err();
    DiskBlockRef pBlock;
    FSContainerInfo fscInfo;
    const TimeInterval curTime = FSGetCurrentTime();

    if ((err = FSContainer_GetInfo(fsContainer, &fscInfo)) != EOK) {
        return err;
    }

    // Make sure that the  disk is compatible with our FS
    if (fscInfo.blockSize != kSFSBlockSize) {
        return EINVAL;
    }
    if (fscInfo.blockCount < kSFSVolume_MinBlockCount) {
        return ENOSPC;
    }


    // Structure of the initialized FS:
    // LBA  
    // 0        Volume Header Block
    // 1        Allocation Bitmap Block #0
    // .        ...
    // Nab      Allocation Bitmap Block #Nab-1
    // Nab+1    Root Directory Inode
    // Nab+2    Root Directory Contents Block #0
    // Nab+3    Unused
    // .        ...
    // Figure out the size and location of the allocation bitmap and root directory
    const uint32_t allocationBitmapByteSize = (fscInfo.blockCount + 7) >> 3;
    const LogicalBlockCount allocBitmapBlockCount = (allocationBitmapByteSize + (fscInfo.blockSize - 1)) / fscInfo.blockSize;
    const LogicalBlockAddress rootDirInodeLba = allocBitmapBlockCount + 1;
    const LogicalBlockAddress rootDirContentLba = rootDirInodeLba + 1;


    // Write the volume header
    try(FSContainer_AcquireBlock(fsContainer, 0, kAcquireBlock_Cleared, &pBlock));
    SFSVolumeHeader* vhp = (SFSVolumeHeader*)DiskBlock_GetMutableData(pBlock);
    vhp->signature = UInt32_HostToBig(kSFSSignature_SerenaFS);
    vhp->version = UInt32_HostToBig(kSFSVersion_Current);
    vhp->attributes = UInt32_HostToBig(0);
    vhp->creationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    vhp->creationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    vhp->modificationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    vhp->modificationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    vhp->blockSize = UInt32_HostToBig(fscInfo.blockSize);
    vhp->volumeBlockCount = UInt32_HostToBig(fscInfo.blockCount);
    vhp->allocationBitmapByteSize = UInt32_HostToBig(allocationBitmapByteSize);
    vhp->rootDirectoryLba = UInt32_HostToBig(rootDirInodeLba);
    vhp->allocationBitmapLba = UInt32_HostToBig(1);
    try(FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync));


    // Write the allocation bitmap
    // Note that we mark the blocks that we already know are in use as in-use
    const size_t nAllocationBitsPerBlock = fscInfo.blockSize << 3;
    const LogicalBlockAddress nBlocksToAllocate = 1 + allocBitmapBlockCount + 1 + 1; // volume header + alloc bitmap + root dir inode + root dir content
    LogicalBlockAddress nBlocksAllocated = 0;

    for (LogicalBlockAddress i = 0; i < allocBitmapBlockCount; i++) {
        try(FSContainer_AcquireBlock(fsContainer, 1 + i, kAcquireBlock_Cleared, &pBlock));
        uint8_t* bbp = DiskBlock_GetMutableData(pBlock);
        LogicalBlockAddress bitNo = 0;

        while (nBlocksAllocated < __min(nBlocksToAllocate, nAllocationBitsPerBlock)) {
            AllocationBitmap_SetBlockInUse(bbp, bitNo, true);
            nBlocksAllocated++;
            bitNo++;
        }

        try(FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync));
    }


    // Write the root directory inode
    try(FSContainer_AcquireBlock(fsContainer, rootDirInodeLba, kAcquireBlock_Cleared, &pBlock));
    SFSInode* ip = (SFSInode*)DiskBlock_GetMutableData(pBlock);
    ip->accessTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->accessTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->modificationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->modificationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->statusChangeTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->statusChangeTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->size = Int64_HostToBig(2 * sizeof(SFSDirectoryEntry));
    ip->uid = UInt32_HostToBig(uid);
    ip->gid = UInt32_HostToBig(gid);
    ip->linkCount = Int32_HostToBig(1);
    ip->permissions = UInt16_HostToBig(permissions);
    ip->type = kFileType_Directory;
    ip->bp[0] = UInt32_HostToBig(rootDirContentLba);
    try(FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync));


    // Write the root directory content. This is just the entries '.' and '..'
    // which both point back to the root directory.
    try(FSContainer_AcquireBlock(fsContainer, rootDirContentLba, kAcquireBlock_Cleared, &pBlock));
    SFSDirectoryEntry* dep = (SFSDirectoryEntry*)DiskBlock_GetMutableData(pBlock);
    dep[0].id = UInt32_HostToBig(rootDirInodeLba);
    dep[0].filename[0] = '.';
    dep[1].id = UInt32_HostToBig(rootDirInodeLba);
    dep[1].filename[0] = '.';
    dep[1].filename[1] = '.';
    try(FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync));

catch:
    return err;
}
