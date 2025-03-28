//
//  SfsAllocator.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SfsAllocator.h"
#include <filesystem/FSContainer.h>
#include <filesystem/FSUtilities.h>
#include <System/ByteOrder.h>


void SfsAllocator_Init(SfsAllocator* _Nonnull self)
{
    memset(self, 0, sizeof(SfsAllocator));
    Lock_Init(&self->lock);
}

void SfsAllocator_Deinit(SfsAllocator* _Nonnull self)
{
    Lock_Deinit(&self->lock);
}

errno_t SfsAllocator_Start(SfsAllocator* _Nonnull self, FSContainerRef _Nonnull fsContainer, const sfs_vol_header_t* _Nonnull vhp, size_t blockSize)
{
    decl_try_err();
    const uint32_t volumeBlockCount = UInt32_BigToHost(vhp->volBlockCount);
    const uint32_t allocationBitmapByteSize = UInt32_BigToHost(vhp->allocBitmapByteSize);
    FSBlock blk = {0};

    if (allocationBitmapByteSize < 1 || volumeBlockCount < kSFSVolume_MinBlockCount) {
        return EIO;
    }

    size_t allocBitmapByteSize = allocationBitmapByteSize;
    self->bitmapLba = UInt32_BigToHost(vhp->lbaAllocBitmap);
    self->bitmapBlockCount = (allocBitmapByteSize + (blockSize - 1)) / blockSize;
    self->bitmapByteSize = allocBitmapByteSize;
    self->blockSize = blockSize;
    self->volumeBlockCount = volumeBlockCount;

    try(FSAllocateCleared((self->bitmapBlockCount + 7) >> 3, (void**)&self->dirtyBitmapBlocks));
    try(FSAllocate(allocBitmapByteSize, (void**)&self->bitmap));
    uint8_t* pAllocBitmap = self->bitmap;


    for (LogicalBlockAddress lba = 0; lba < self->bitmapBlockCount; lba++) {
        const size_t nBytesToCopy = __min(blockSize, allocBitmapByteSize);

        try(FSContainer_MapBlock(fsContainer, self->bitmapLba + lba, kMapBlock_ReadOnly, &blk));
        memcpy(pAllocBitmap, blk.data, nBytesToCopy);
        FSContainer_UnmapBlock(fsContainer, blk.token);
        blk.token = 0;
        blk.data = NULL;

        allocBitmapByteSize -= nBytesToCopy;
        pAllocBitmap += blockSize;
    }

catch:
    return err;
}

void SfsAllocator_Stop(SfsAllocator* _Nonnull self)
{
    FSDeallocate(self->dirtyBitmapBlocks);
    self->dirtyBitmapBlocks = NULL;

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

errno_t SfsAllocator_Allocate(SfsAllocator* _Nonnull self, LogicalBlockAddress* _Nonnull pOutLba)
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
    AllocationBitmap_SetBlockInUse(self->dirtyBitmapBlocks, (lba >> 3) / self->blockSize, true);
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

void SfsAllocator_Deallocate(SfsAllocator* _Nonnull self, LogicalBlockAddress lba)
{
    if (lba == 0) {
        return;
    }

    Lock_Lock(&self->lock);
    AllocationBitmap_SetBlockInUse(self->bitmap, lba, false);
    AllocationBitmap_SetBlockInUse(self->dirtyBitmapBlocks, (lba >> 3) / self->blockSize, true);
    Lock_Unlock(&self->lock);
}

errno_t SfsAllocator_CommitToDisk(SfsAllocator* _Nonnull self, FSContainerRef _Nonnull fsContainer)
{
    decl_try_err();
    FSBlock blk = {0};

    Lock_Lock(&self->lock);

    for (LogicalBlockAddress i = 0; i < self->bitmapBlockCount; i++) {
        if (AllocationBitmap_IsBlockInUse(self->dirtyBitmapBlocks, i)) {
            const LogicalBlockAddress allocationBitmapBlockLba = self->bitmapLba + i;
            const uint8_t* pBitmapData = &self->bitmap[i * self->blockSize];

            if ((err = FSContainer_MapBlock(fsContainer, allocationBitmapBlockLba, kMapBlock_Cleared, &blk)) == EOK) {
                memcpy(blk.data, pBitmapData, self->bitmapByteSize);
                FSContainer_UnmapBlockWriting(fsContainer, blk.token, kWriteBlock_Deferred);
            }
            
            blk.token = 0;
            blk.data = NULL;

            if (err != EOK) {
                break;
            }

            AllocationBitmap_SetBlockInUse(self->dirtyBitmapBlocks, i, false);
        }
    }

    Lock_Unlock(&self->lock);

    return err;
}
