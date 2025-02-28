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
    FSContainerInfo diskinf;
    const TimeInterval curTime = FSGetCurrentTime();

    if ((err = FSContainer_GetInfo(fsContainer, &diskinf)) != EOK) {
        return err;
    }

    // Make sure that the disk is compatible with our FS
    if (!FSIsPowerOf2(diskinf.blockSize)) {
        return EINVAL;
    }
    if (diskinf.blockSize < kSFSVolume_MinBlockSize) {
        return EINVAL;
    }
    if (diskinf.blockCount < kSFSVolume_MinBlockCount) {
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
    const uint32_t allocationBitmapByteSize = (diskinf.blockCount + 7) >> 3;
    const LogicalBlockCount allocBitmapBlockCount = (allocationBitmapByteSize + (diskinf.blockSize - 1)) / diskinf.blockSize;
    const LogicalBlockAddress rootDirLba = allocBitmapBlockCount + 1;
    const LogicalBlockAddress rootDirContLba = rootDirLba + 1;


    // Write the volume header
    try(FSContainer_AcquireBlock(fsContainer, 0, kAcquireBlock_Cleared, &pBlock));
    sfs_vol_header_t* vhp = (sfs_vol_header_t*)DiskBlock_GetMutableData(pBlock);
    vhp->signature = UInt32_HostToBig(kSFSSignature_SerenaFS);
    vhp->version = UInt32_HostToBig(kSFSVersion_Current);
    vhp->attributes = UInt32_HostToBig(0);
    vhp->creationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    vhp->creationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    vhp->modificationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    vhp->modificationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    vhp->volBlockSize = UInt32_HostToBig(diskinf.blockSize);
    vhp->volBlockCount = UInt32_HostToBig(diskinf.blockCount);
    vhp->allocBitmapByteSize = UInt32_HostToBig(allocationBitmapByteSize);
    vhp->lbaRootDir = UInt32_HostToBig(rootDirLba);
    vhp->lbaAllocBitmap = UInt32_HostToBig(1);
    try(FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync));


    // Write the allocation bitmap
    // Note that we mark the blocks that we already know are in use as in-use
    const size_t nAllocationBitsPerBlock = diskinf.blockSize << 3;
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
    try(FSContainer_AcquireBlock(fsContainer, rootDirLba, kAcquireBlock_Cleared, &pBlock));
    sfs_inode_t* ip = (sfs_inode_t*)DiskBlock_GetMutableData(pBlock);
    ip->accessTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->accessTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->modificationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->modificationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->statusChangeTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->statusChangeTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->size = Int64_HostToBig(2 * sizeof(sfs_dirent_t));
    ip->uid = UInt32_HostToBig(uid);
    ip->gid = UInt32_HostToBig(gid);
    ip->linkCount = Int32_HostToBig(1);
    ip->permissions = UInt16_HostToBig(permissions);
    ip->type = kFileType_Directory;
    ip->bmap.direct[0] = UInt32_HostToBig(rootDirContLba);
    try(FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync));


    // Write the root directory content. This is just the entries '.' and '..'
    // which both point back to the root directory.
    try(FSContainer_AcquireBlock(fsContainer, rootDirContLba, kAcquireBlock_Cleared, &pBlock));
    sfs_dirent_t* dep = (sfs_dirent_t*)DiskBlock_GetMutableData(pBlock);
    dep[0].id = UInt32_HostToBig(rootDirLba);
    dep[0].len = 1;
    dep[0].filename[0] = '.';
    dep[1].id = UInt32_HostToBig(rootDirLba);
    dep[1].len = 2;
    dep[1].filename[0] = '.';
    dep[1].filename[1] = '.';
    try(FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync));

catch:
    return err;
}
