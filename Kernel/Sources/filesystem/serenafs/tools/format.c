//
//  SerenaFS_Format.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <System/ByteOrder.h>
#include <System/Error.h>
#include <System/File.h>
#include <System/Types.h>
#include <System/_math.h>
#include <filesystem/FSContainer.h>
#include <filesystem/FSUtilities.h>
#include <filesystem/serenafs/VolumeFormat.h>
#ifdef _WIN32
#include <stdlib.h>
#endif


errno_t block_write(intptr_t fd, const void* _Nonnull buf, LogicalBlockAddress blockAddr, size_t blockSize)
{
#ifdef __DISKIMAGE__
    extern errno_t RamFSContainer_Write(void* _Nonnull self, const void* _Nonnull buf, ssize_t nBytesToWrite, off_t offset, ssize_t* _Nonnull pOutBytesWritten);

    ssize_t bytesWritten;
    const errno_t err = RamFSContainer_Write((void*)fd, buf, blockSize, blockAddr * blockSize, &bytesWritten);

    return (err == EOK && bytesWritten == blockSize) ? EOK : EIO;
#else
    return EIO;
#endif
}

// Formats the given disk drive and installs a SerenaFS with an empty root
// directory on it. 'user' and 'permissions' are the user and permissions that
// should be assigned to the root directory.
errno_t sefs_format(intptr_t fd, LogicalBlockCount blockCount, size_t blockSize, uid_t uid, gid_t gid, FilePermissions permissions, const char* _Nonnull label)
{
    decl_try_err();
    const TimeInterval curTime = FSGetCurrentTime();


    // Make sure that the disk is compatible with our FS
    if (!FSIsPowerOf2(blockSize)) {
        return EINVAL;
    }
    if (blockSize < kSFSVolume_MinBlockSize) {
        return EINVAL;
    }
    if (blockCount < kSFSVolume_MinBlockCount) {
        return ENOSPC;
    }
    if (strlen(label) > kSFSMaxVolumeLabelLength) {
        return ERANGE;
    }


    void* bp = malloc(blockSize);


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
    const uint32_t allocationBitmapByteSize = (blockCount + 7) >> 3;
    const LogicalBlockCount allocBitmapBlockCount = (allocationBitmapByteSize + (blockSize - 1)) / blockSize;
    const LogicalBlockAddress rootDirLba = allocBitmapBlockCount + 1;
    const LogicalBlockAddress rootDirContLba = rootDirLba + 1;


    // Write the volume header
    sfs_vol_header_t* vhp = (sfs_vol_header_t*)bp;
    memset(bp, 0, blockSize);
    vhp->signature = UInt32_HostToBig(kSFSSignature_SerenaFS);
    vhp->version = UInt32_HostToBig(kSFSVersion_Current);
    vhp->attributes = UInt32_HostToBig(0);
    vhp->creationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    vhp->creationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    vhp->modificationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    vhp->modificationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    vhp->volBlockSize = UInt32_HostToBig(blockSize);
    vhp->volBlockCount = UInt32_HostToBig(blockCount);
    vhp->allocBitmapByteSize = UInt32_HostToBig(allocationBitmapByteSize);
    vhp->lbaRootDir = UInt32_HostToBig(rootDirLba);
    vhp->lbaAllocBitmap = UInt32_HostToBig(1);
    vhp->labelLength = strlen(label);
    memcpy(vhp->label, label, vhp->labelLength);
    try(block_write(fd, bp, 0, blockSize));


    // Write the allocation bitmap
    // Note that we mark the blocks that we already know are in use as in-use
    const size_t nAllocationBitsPerBlock = blockSize << 3;
    const LogicalBlockAddress nBlocksToAllocate = 1 + allocBitmapBlockCount + 1 + 1; // volume header + alloc bitmap + root dir inode + root dir content
    LogicalBlockAddress nBlocksAllocated = 0;

    for (LogicalBlockAddress i = 0; i < allocBitmapBlockCount; i++) {
        uint8_t* bbp = bp;
        LogicalBlockAddress bitNo = 0;

        memset(bbp, 0, blockSize);
        while (nBlocksAllocated < __min(nBlocksToAllocate, nAllocationBitsPerBlock)) {
            AllocationBitmap_SetBlockInUse(bbp, bitNo, true);
            nBlocksAllocated++;
            bitNo++;
        }

        try(block_write(fd, bp, 1 + i, blockSize));
    }


    // Write the root directory inode
    sfs_inode_t* ip = (sfs_inode_t*)bp;
    memset(ip, 0, blockSize);
    ip->size = Int64_HostToBig(2 * sizeof(sfs_dirent_t));
    ip->accessTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->accessTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->modificationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->modificationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->statusChangeTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->statusChangeTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->signature = UInt32_HostToBig(kSFSSignature_Inode);
    ip->id = UInt32_HostToBig(rootDirLba);
    ip->pnid = UInt32_HostToBig(rootDirLba);
    ip->linkCount = Int32_HostToBig(1);
    ip->uid = UInt32_HostToBig(uid);
    ip->gid = UInt32_HostToBig(gid);
    ip->permissions = UInt16_HostToBig(permissions);
    ip->type = kFileType_Directory;
    ip->bmap.direct[0] = UInt32_HostToBig(rootDirContLba);
    try(block_write(fd, bp, rootDirLba, blockSize));


    // Write the root directory content. This is just the entries '.' and '..'
    // which both point back to the root directory.
    sfs_dirent_t* dep = (sfs_dirent_t*)bp;
    memset(dep, 0, blockSize);
    dep[0].id = UInt32_HostToBig(rootDirLba);
    dep[0].len = 1;
    dep[0].filename[0] = '.';
    dep[1].id = UInt32_HostToBig(rootDirLba);
    dep[1].len = 2;
    dep[1].filename[0] = '.';
    dep[1].filename[1] = '.';
    try(block_write(fd, bp, rootDirContLba, blockSize));

catch:
    free(bp);
    return err;
}
