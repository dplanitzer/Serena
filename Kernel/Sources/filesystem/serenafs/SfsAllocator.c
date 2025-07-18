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
#include <kern/endian.h>
#include <kern/string.h>


void SfsAllocator_Init(SfsAllocator* _Nonnull self)
{
    memset(self, 0, sizeof(SfsAllocator));
    mtx_init(&self->mtx);
}

void SfsAllocator_Deinit(SfsAllocator* _Nonnull self)
{
    mtx_deinit(&self->mtx);
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


    for (blkno_t lba = 0; lba < self->bitmapBlockCount; lba++) {
        const size_t nBytesToCopy = __min(blockSize, allocBitmapByteSize);

        try(FSContainer_MapBlock(fsContainer, self->bitmapLba + lba, kMapBlock_ReadOnly, &blk));
        memcpy(pAllocBitmap, blk.data, nBytesToCopy);
        FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_None);
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
static bool AllocationBitmap_IsBlockInUse(const uint8_t *bitmap, blkno_t lba)
{
    return ((bitmap[lba >> 3] & (1 << (7 - (lba & 0x07)))) != 0) ? true : false;
}

// Sets the in-use bit corresponding to the logical block address 'lba' as in-use or not
void AllocationBitmap_SetBlockInUse(uint8_t *bitmap, blkno_t lba, bool inUse)
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

errno_t SfsAllocator_Allocate(SfsAllocator* _Nonnull self, blkno_t* _Nonnull pOutLba)
{
    decl_try_err();
    blkno_t lba = 0;    // Safe because LBA #0 is the volume header which is always allocated when the FS is mounted

    mtx_lock(&self->mtx);

    for (blkno_t i = 1; i < self->volumeBlockCount; i++) {
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
    mtx_unlock(&self->mtx);

    *pOutLba = lba;
    return EOK;

catch:
    if (lba > 0) {
        AllocationBitmap_SetBlockInUse(self->bitmap, lba, false);
    }
    mtx_unlock(&self->mtx);
    *pOutLba = 0;

    return err;
}

void SfsAllocator_Deallocate(SfsAllocator* _Nonnull self, blkno_t lba)
{
    if (lba == 0) {
        return;
    }

    mtx_lock(&self->mtx);
    AllocationBitmap_SetBlockInUse(self->bitmap, lba, false);
    AllocationBitmap_SetBlockInUse(self->dirtyBitmapBlocks, (lba >> 3) / self->blockSize, true);
    mtx_unlock(&self->mtx);
}

blkcnt_t SfsAllocator_GetAllocatedBlockCount(SfsAllocator* _Nonnull self)
{
    decl_try_err();
    blkcnt_t count = 0;

    mtx_lock(&self->mtx);

    for (blkno_t i = 0; i < self->volumeBlockCount; i++) {
        if (AllocationBitmap_IsBlockInUse(self->bitmap, i)) {
            count++;
        }
    }

    mtx_unlock(&self->mtx);

    return count;
}

errno_t SfsAllocator_CommitToDisk(SfsAllocator* _Nonnull self, FSContainerRef _Nonnull fsContainer)
{
    decl_try_err();
    FSBlock blk = {0};

    mtx_lock(&self->mtx);

    for (blkno_t i = 0; i < self->bitmapBlockCount; i++) {
        if (AllocationBitmap_IsBlockInUse(self->dirtyBitmapBlocks, i)) {
            const blkno_t allocationBitmapBlockLba = self->bitmapLba + i;
            const uint8_t* pBitmapData = &self->bitmap[i * self->blockSize];

            if ((err = FSContainer_MapBlock(fsContainer, allocationBitmapBlockLba, kMapBlock_Cleared, &blk)) == EOK) {
                memcpy(blk.data, pBitmapData, self->bitmapByteSize);
                FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_Deferred);
            }
            
            blk.token = 0;
            blk.data = NULL;

            if (err != EOK) {
                break;
            }

            AllocationBitmap_SetBlockInUse(self->dirtyBitmapBlocks, i, false);
        }
    }

    mtx_unlock(&self->mtx);

    return err;
}
