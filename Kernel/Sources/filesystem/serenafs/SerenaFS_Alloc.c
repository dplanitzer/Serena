//
//  SerenaFS_Alloc.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <System/ByteOrder.h>


void BlockAllocator_Init(BlockAllocator* _Nonnull self, FSContainerRef _Nonnull fsContainer)
{
    memset(self, 0, sizeof(BlockAllocator));
    Lock_Init(&self->lock);
    self->fsContainer = fsContainer;
}

void BlockAllocator_Deinit(BlockAllocator* _Nonnull self)
{
    self->fsContainer = NULL;
    Lock_Deinit(&self->lock);
}

errno_t BlockAllocator_Start(BlockAllocator* _Nonnull self, const SFSVolumeHeader* _Nonnull vhp, size_t blockSize)
{
    decl_try_err();
    const uint32_t volumeBlockCount = UInt32_BigToHost(vhp->volumeBlockCount);
    const uint32_t allocationBitmapByteSize = UInt32_BigToHost(vhp->allocationBitmapByteSize);

    if (allocationBitmapByteSize < 1 || volumeBlockCount < kSFSVolume_MinBlockCount) {
        return EIO;
    }

    size_t allocBitmapByteSize = allocationBitmapByteSize;
    self->bitmapLba = UInt32_BigToHost(vhp->allocationBitmapLba);
    self->bitmapBlockCount = (allocBitmapByteSize + (blockSize - 1)) / blockSize;
    self->bitmapByteSize = allocBitmapByteSize;
    self->volumeBlockCount = volumeBlockCount;


    try(FSAllocate(allocBitmapByteSize, (void**)&self->bitmap));
    uint8_t* pAllocBitmap = self->bitmap;


    for (LogicalBlockAddress lba = 0; lba < self->bitmapBlockCount; lba++) {
        const size_t nBytesToCopy = __min(kSFSBlockSize, allocBitmapByteSize);
        DiskBlockRef pBlock;

        try(FSContainer_AcquireBlock(self->fsContainer, self->bitmapLba + lba, kAcquireBlock_ReadOnly, &pBlock));
        memcpy(pAllocBitmap, DiskBlock_GetData(pBlock), nBytesToCopy);
        FSContainer_RelinquishBlock(self->fsContainer, pBlock);
        pBlock = NULL;

        allocBitmapByteSize -= nBytesToCopy;
        pAllocBitmap += blockSize;
    }

catch:
    return err;
}

void BlockAllocator_Stop(BlockAllocator* _Nonnull self)
{
    FSDeallocate(self->bitmap);
    self->bitmap = NULL;

    self->bitmapBlockCount = 0;
    self->bitmapByteSize = 0;
    self->bitmapLba = 0;
    self->volumeBlockCount = 0;
}

// Returns true if the allocation block 'lba' is in use and false otherwise
static bool AllocationBitmap_IsBlockInUse(const uint8_t *bitmap, LogicalBlockAddress lba)
{
    return ((bitmap[lba >> 3] & (1 << (7 - (lba & 0x07)))) != 0) ? true : false;
}

// Sets the in-use bit corresponding to the logical block address 'lba' as in-use or not
void AllocationBitmap_SetBlockInUse(uint8_t *bitmap, LogicalBlockAddress lba, bool inUse)
{
    uint8_t* bytePtr = &bitmap[lba >> 3];
    const uint8_t bitNo = 7 - (lba & 0x07);

    if (inUse) {
        *bytePtr |= (1 << bitNo);
    }
    else {
        *bytePtr &= ~(1 << bitNo);
    }
}


static errno_t SerenaFS_WriteBackAllocationBitmapForLba(BlockAllocator* _Nonnull self, LogicalBlockAddress lba)
{
    decl_try_err();
    const LogicalBlockAddress idxOfAllocBitmapBlockModified = (lba >> 3) / kSFSBlockSize;
    const uint8_t* pBitmapData = &self->bitmap[idxOfAllocBitmapBlockModified * kSFSBlockSize];
    const LogicalBlockAddress allocationBitmapBlockLba = self->bitmapLba + idxOfAllocBitmapBlockModified;
    DiskBlockRef pBlock;

    if ((err = FSContainer_AcquireBlock(self->fsContainer, allocationBitmapBlockLba, kAcquireBlock_Cleared, &pBlock)) == EOK) {
        memcpy(DiskBlock_GetMutableData(pBlock), pBitmapData, self->bitmapByteSize);
        FSContainer_RelinquishBlockWriting(self->fsContainer, pBlock, kWriteBlock_Sync);
    }
    return err;
}

errno_t BlockAllocator_Allocate(BlockAllocator* _Nonnull self, LogicalBlockAddress* _Nonnull pOutLba)
{
    decl_try_err();
    LogicalBlockAddress lba = 0;    // Safe because LBA #0 is the volume header which is always allocated when the FS is mounted

    Lock_Lock(&self->lock);

    for (LogicalBlockAddress i = 1; i < self->volumeBlockCount; i++) {
        if (!AllocationBitmap_IsBlockInUse(self->bitmap, i)) {
            lba = i;
            break;
        }
    }
    if (lba == 0) {
        throw(ENOSPC);
    }

    AllocationBitmap_SetBlockInUse(self->bitmap, lba, true);
    try(SerenaFS_WriteBackAllocationBitmapForLba(self, lba));
    Lock_Unlock(&self->lock);

    *pOutLba = lba;
    return EOK;

catch:
    if (lba > 0) {
        AllocationBitmap_SetBlockInUse(self->bitmap, lba, false);
    }
    Lock_Unlock(&self->lock);
    *pOutLba = 0;
    return err;
}

void BlockAllocator_Deallocate(BlockAllocator* _Nonnull self, LogicalBlockAddress lba)
{
    if (lba == 0) {
        return;
    }

    Lock_Lock(&self->lock);
    AllocationBitmap_SetBlockInUse(self->bitmap, lba, false);

    // XXX check for error here?
    SerenaFS_WriteBackAllocationBitmapForLba(self, lba);
    Lock_Unlock(&self->lock);
}
