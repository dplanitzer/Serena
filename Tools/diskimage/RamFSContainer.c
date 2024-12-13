//
//  RamFSContainer.c
//  diskimage
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RamFSContainer.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem/SerenaDiskImage.h>
#include <System/ByteOrder.h>
#include <System/Disk.h>


errno_t RamFSContainer_Create(const DiskImageFormat* _Nonnull pFormat, RamFSContainerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RamFSContainerRef self;

    try(Object_Create(RamFSContainer, &self));
    self->diskImage = calloc(pFormat->blocksPerDisk, pFormat->blockSize);
    self->blockSize = pFormat->blockSize;
    self->blockCount = pFormat->blocksPerDisk;
    self->highestWrittenToLba = 0;
    self->format = pFormat->format;

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

void RamFSContainer_deinit(RamFSContainerRef _Nonnull self)
{
    free(self->diskImage);
    self->diskImage = NULL;
}

errno_t RamFSContainer_getInfo(RamFSContainerRef _Nonnull self, FSContainerInfo* pOutInfo)
{
    pOutInfo->blockSize = self->blockSize;
    pOutInfo->blockCount = self->blockCount;
    pOutInfo->isReadOnly = false;

    return EOK;
}

errno_t RamFSContainer_acquireEmptyBlock(RamFSContainerRef self, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    DiskBlockRef pBlock;
    const errno_t err = DiskBlock_Create(kDiskId_None, kMediaId_None, 0, &pBlock);

    if (err == EOK) {
        memset(DiskBlock_GetMutableData(pBlock), 0, DiskBlock_GetByteSize(pBlock));
    }
    *pOutBlock = pBlock;
    return err;
}

static errno_t RamFSContainer_GetBlock(RamFSContainerRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    if (lba >= self->blockCount) {
        return EIO;
    }

    memcpy(pBuffer, &self->diskImage[lba * self->blockSize], self->blockSize);
    return EOK;
}

errno_t RamFSContainer_acquireBlock(struct RamFSContainer* _Nonnull self, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    err = DiskBlock_Create(1, 1, lba, &pBlock);
    if (err == EOK) {
        switch (mode) {
            case kAcquireBlock_ReadOnly:
            case kAcquireBlock_Update:
                err = RamFSContainer_GetBlock(self, DiskBlock_GetMutableData(pBlock), DiskBlock_GetLba(pBlock));
                break;

            case kAcquireBlock_Replace:
                // Caller will replace every byte in the block
                break;

            case kAcquireBlock_Cleared:
                memset(DiskBlock_GetMutableData(pBlock), 0, DiskBlock_GetByteSize(pBlock));
                break; 

        }
    }
    *pOutBlock = pBlock;

    return err;
}

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
static errno_t RamFSContainer_PutBlock(RamFSContainerRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    if (lba >= self->blockCount) {
        return EIO;
    }

    memcpy(&self->diskImage[lba * self->blockSize], pBuffer, self->blockSize);
    if (lba > self->highestWrittenToLba) {
        self->highestWrittenToLba = lba;
    }

    return EOK;
}

errno_t RamFSContainer_relinquishBlockWriting(struct RamFSContainer* _Nonnull self, DiskBlockRef _Nullable pBlock, WriteBlock mode)
{
    decl_try_err();

    if (pBlock) {
        switch (mode) {
            case kWriteBlock_Sync:
            case kWriteBlock_Async:
            case kWriteBlock_Deferred:
                err = RamFSContainer_PutBlock(self, DiskBlock_GetData(pBlock), DiskBlock_GetLba(pBlock));
                break;

            default:
                abort();
        }
        DiskBlock_Destroy(pBlock);
    }

    return err;
}

void RamFSContainer_relinquishBlock(struct RamFSContainer* _Nonnull self, DiskBlockRef _Nullable pBlock)
{
    if (pBlock) {
        DiskBlock_Destroy(pBlock);
    }
}

// Writes the contents of the disk to the given path as a regular file.
errno_t RamFSContainer_WriteToPath(RamFSContainerRef _Nonnull self, const char* pPath)
{
    decl_try_err();
    FILE* fp;

    try_null(fp, fopen(pPath, "wb"), EIO);
    
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
override_func_def(acquireBlock, RamFSContainer, FSContainer)
override_func_def(relinquishBlockWriting, RamFSContainer, FSContainer)
override_func_def(relinquishBlock, RamFSContainer, FSContainer)
);
