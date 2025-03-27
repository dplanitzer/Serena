//
//  RamFSContainer.c
//  diskimage
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RamFSContainer.h"
#include "diskimage.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem/SerenaDiskImage.h>
#include <filesystem/FSUtilities.h>
#include <System/ByteOrder.h>
#include <System/Disk.h>


errno_t RamFSContainer_Create(const DiskImageFormat* _Nonnull format, RamFSContainerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RamFSContainerRef self;

    try(Object_Create(class(RamFSContainer), 0, (void**)&self));
    try(FSAllocateCleared(format->blocksPerDisk * format->blockSize, (void**)&self->diskImage));
    try(FSAllocateCleared(format->blockSize, (void**)&self->emptyBlock));
    try(FSAllocateCleared(format->blocksPerDisk, (void**)&self->mappedFlags));
    self->blockSize = format->blockSize;
    self->blockShift = FSLog2(self->blockSize);
    self->blockMask = self->blockSize - 1;
    self->blockCount = format->blocksPerDisk;
    self->lowestWrittenToLba = ~0;
    self->highestWrittenToLba = 0;
    self->format = format->format;

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t RamFSContainer_CreateWithContentsOfPath(const char* _Nonnull path, RamFSContainerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FILE* fp = NULL;
    RamFSContainerRef self = NULL;
    DiskImageFormat dif;
    DiskImage fmt;

    try(di_describe_diskimage(path, &fmt));
    dif.name = "";
    dif.blockSize = fmt.bytesPerSector;
    dif.blocksPerDisk = fmt.sectorsPerTrack * fmt.headsPerCylinder * fmt.cylindersPerDisk;
    dif.format = fmt.format;

    try(RamFSContainer_Create(&dif, &self));

    try_null(fp, fopen(path, "rb"), errno);
    if (fread(self->diskImage, self->blockSize, self->blockCount, fp) != self->blockCount) {
        throw(EIO);
    }
    fclose(fp);

    *pOutSelf = self;
    return EOK;

catch:
    if (fp) {
        fclose(fp);
    }
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

void RamFSContainer_deinit(RamFSContainerRef _Nonnull self)
{
    FSDeallocate(self->mappedFlags);
    self->mappedFlags = NULL;

    FSDeallocate(self->emptyBlock);
    self->emptyBlock = NULL;

    FSDeallocate(self->diskImage);
    self->diskImage = NULL;
}

errno_t RamFSContainer_getInfo(RamFSContainerRef _Nonnull self, FSContainerInfo* pOutInfo)
{
    pOutInfo->blockSize = self->blockSize;
    pOutInfo->blockCount = self->blockCount;
    pOutInfo->isReadOnly = false;

    return EOK;
}

errno_t RamFSContainer_mapEmptyBlock(RamFSContainerRef self, FSBlock* _Nonnull blk)
{
    blk->token = 0;
    blk->data = self->emptyBlock;

    return EOK;
}

errno_t RamFSContainer_mapBlock(struct RamFSContainer* _Nonnull self, LogicalBlockAddress lba, AcquireBlock mode, FSBlock* _Nonnull blk)
{
    decl_try_err();

    if (self->mappedFlags[lba]) {
        abort();
        //XXX for now. Should really wait until the block has been unmapped
    }

    switch (mode) {
        case kAcquireBlock_ReadOnly:
        case kAcquireBlock_Update:
        case kAcquireBlock_Replace:
        case kAcquireBlock_Cleared:
            if (lba < self->blockCount) {
                blk->token = lba + 1;
                blk->data = &self->diskImage[lba << self->blockShift];
            }
            else {
                err = ENXIO;
            }
            break;

        default:
            abort();
            break;
    }

    if (err == EOK) {
        if (mode == kAcquireBlock_Cleared) {
            memset(blk->data, 0, self->blockSize);
        }

        self->mappedFlags[lba] = true;
    }

    return err;
}

void RamFSContainer_unmapBlock(struct RamFSContainer* _Nonnull self, intptr_t token)
{
    if (token) {
        self->mappedFlags[token - 1] = false;
    }
}

// Writes the contents of 'buf' to the block at index 'lba'. 'buf'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
errno_t RamFSContainer_unmapBlockWriting(struct RamFSContainer* _Nonnull self, intptr_t token, WriteBlock mode)
{
    decl_try_err();

    if (token == 0) {
        return EOK;
    }

    LogicalBlockAddress lba = token - 1;
    switch (mode) {
        case kWriteBlock_Sync:
        case kWriteBlock_Deferred:
            if (lba < self->blockCount) {
                if (lba < self->lowestWrittenToLba) {
                    self->lowestWrittenToLba = lba;
                }
                if (lba > self->highestWrittenToLba) {
                    self->highestWrittenToLba = lba;
                }

                self->mappedFlags[lba] = false;
            }
            else {
                err = ENXIO;
            }
            break;

        default:
            abort();
            break;
    }

    return err;
}

static void convert_offset(struct RamFSContainer* _Nonnull self, off_t offset, LogicalBlockAddress* _Nonnull pOutBlockIdx, ssize_t* _Nonnull pOutBlockOffset)
{
    *pOutBlockIdx = (LogicalBlockAddress)(offset >> (off_t)self->blockShift);
    *pOutBlockOffset = (ssize_t)(offset & (off_t)self->blockMask);
}

errno_t RamFSContainer_Read(RamFSContainerRef _Nonnull self, void* _Nonnull buf, ssize_t nBytesToRead, off_t offset, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    const off_t diskSize = (off_t)self->blockCount << (off_t)self->blockShift;
    uint8_t* dp = buf;
    ssize_t nBytesRead = 0;

    if (nBytesToRead < 0) {
        throw(EINVAL);
    }
    if (nBytesToRead > 0) {
        if (offset < 0ll) {
            throw(EOVERFLOW);
        }
        if (offset >= diskSize) {
            throw(ENXIO);
        }
    }


    // Limit 'nBytesToRead' to the range of bytes that is actually available
    // starting at 'offset'.
    const off_t nAvailBytes = diskSize - offset;

    if (nAvailBytes > 0) {
        if (nAvailBytes <= (off_t)SSIZE_MAX && (ssize_t)nAvailBytes < nBytesToRead) {
            nBytesToRead = (ssize_t)nAvailBytes;
        }
        // Otherwise, use 'nBytesToRead' as is
    } else {
        nBytesToRead = 0;
    }


    // Get the block index and block offset that corresponds to 'offset'. We start
    // iterating through blocks there.
    LogicalBlockAddress blockIdx;
    ssize_t blockOffset;
    convert_offset(self, offset, &blockIdx, &blockOffset);


    // Iterate through a contiguous sequence of blocks until we've read all
    // required bytes.
    while (nBytesToRead > 0) {
        const ssize_t nRemainderBlockSize = (ssize_t)self->blockSize - blockOffset;
        const ssize_t nBytesToReadInBlock = (nBytesToRead > nRemainderBlockSize) ? nRemainderBlockSize : nBytesToRead;
        FSBlock blk = {0};

        errno_t e1 = FSContainer_MapBlock(self, blockIdx, kAcquireBlock_ReadOnly, &blk);
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        memcpy(dp, blk.data + blockOffset, nBytesToReadInBlock);
        FSContainer_UnmapBlock(self, blk.token);

        nBytesToRead -= nBytesToReadInBlock;
        nBytesRead += nBytesToReadInBlock;
        dp += nBytesToReadInBlock;

        blockOffset = 0;
        blockIdx++;
    }


catch:
    *pOutBytesRead = nBytesRead;
    return err;
}

errno_t RamFSContainer_Write(RamFSContainerRef _Nonnull self, const void* _Nonnull buf, ssize_t nBytesToWrite, off_t offset, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    const off_t diskSize = (off_t)self->blockCount << (off_t)self->blockShift;
    const uint8_t* sp = buf;
    ssize_t nBytesWritten = 0;

    if (nBytesToWrite < 0) {
        throw(EINVAL);
    }
    if (nBytesToWrite > 0) {
        if (offset < 0ll) {
            throw(EOVERFLOW);
        }
        if (offset >= diskSize) {
            throw(ENXIO);
        }
    }


    // Limit 'nBytesToWrite' to the range of bytes that is actually available
    // starting at 'offset'.
    const off_t nAvailBytes = diskSize - offset;

    if (nAvailBytes > 0) {
        if (nAvailBytes <= (off_t)SSIZE_MAX && (ssize_t)nAvailBytes < nBytesToWrite) {
            nBytesToWrite = (ssize_t)nAvailBytes;
        }
        // Otherwise, use 'nBytesToWrite' as is
    } else {
        nBytesToWrite = 0;
    }


    // Get the block index and block offset that corresponds to 'offset'. We start
    // iterating through blocks there.
    LogicalBlockAddress blockIdx;
    ssize_t blockOffset;
    convert_offset(self, offset, &blockIdx, &blockOffset);


    // Iterate through a contiguous sequence of blocks until we've written all
    // required bytes.
    while (nBytesToWrite > 0) {
        const ssize_t nRemainderBlockSize = (ssize_t)self->blockSize - blockOffset;
        const ssize_t nBytesToWriteInBlock = (nBytesToWrite > nRemainderBlockSize) ? nRemainderBlockSize : nBytesToWrite;
        AcquireBlock acquireMode = (nBytesToWriteInBlock == (ssize_t)self->blockSize) ? kAcquireBlock_Replace : kAcquireBlock_Update;
        FSBlock blk = {0};

        errno_t e1 = FSContainer_MapBlock(self, blockIdx, acquireMode, &blk);
        if (e1 == EOK) {
            memcpy(blk.data + blockOffset, sp, nBytesToWriteInBlock);
            e1 = FSContainer_UnmapBlockWriting(self, blk.token, kWriteBlock_Sync);
        }
        if (e1 != EOK) {
            err = (nBytesWritten == 0) ? e1 : EOK;
            break;
        }

        nBytesToWrite -= nBytesToWriteInBlock;
        nBytesWritten += nBytesToWriteInBlock;
        sp += nBytesToWriteInBlock;

        blockOffset = 0;
        blockIdx++;
    }


catch:
    *pOutBytesWritten = nBytesWritten;
    return err;
}

void RamFSContainer_WipeDisk(RamFSContainerRef _Nonnull self)
{
    memset(self->diskImage, 0, self->blockCount << self->blockShift);
    
    self->lowestWrittenToLba = 0;
    self->highestWrittenToLba = self->blockCount - 1;
}

// Writes the contents of the disk to the given path as a regular file.
errno_t RamFSContainer_WriteToPath(RamFSContainerRef _Nonnull self, const char* path)
{
    decl_try_err();
    FILE* fp;

    try_null(fp, fopen(path, "wb"), EIO);
    
    if (self->format == kDiskImage_Serena) {
        SMG_Header  hdr;

        memset(&hdr, 0, sizeof(SMG_Header));
        hdr.signature = UInt32_HostToBig(SMG_SIGNATURE);
        hdr.headerSize = UInt32_HostToBig(SMG_HEADER_SIZE);
        hdr.physicalBlockCount = UInt64_HostToBig(self->blockCount);
        hdr.logicalBlockCount = UInt64_HostToBig((uint64_t)self->highestWrittenToLba + 1ull);
        hdr.blockSize = UInt32_HostToBig(self->blockSize);
        hdr.options = 0;

        fwrite(&hdr, SMG_HEADER_SIZE, 1, fp);
        if (ferror(fp)) {
            throw(EIO);
        }
    }

    LogicalBlockCount nBlocksToWrite = self->blockCount;
    if (self->format == kDiskImage_Serena) {
        nBlocksToWrite = __min(nBlocksToWrite, self->highestWrittenToLba + 1);
    }

    fwrite(self->diskImage, self->blockSize, nBlocksToWrite, fp);
    if (ferror(fp)) {
        throw(EIO);
    }

catch:
    fclose(fp);
    return err;
}


class_func_defs(RamFSContainer, FSContainer,
override_func_def(deinit, RamFSContainer, Object)
override_func_def(getInfo, RamFSContainer, FSContainer)
override_func_def(mapEmptyBlock, RamFSContainer, FSContainer)
override_func_def(mapBlock, RamFSContainer, FSContainer)
override_func_def(unmapBlockWriting, RamFSContainer, FSContainer)
override_func_def(unmapBlock, RamFSContainer, FSContainer)
);
