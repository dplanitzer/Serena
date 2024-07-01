//
//  SerenaFS_Alloc.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"



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


static errno_t SerenaFS_WriteBackAllocationBitmapForLba(SerenaFSRef _Nonnull self, LogicalBlockAddress lba)
{
    const LogicalBlockAddress idxOfAllocBitmapBlockModified = (lba >> 3) / kSFSBlockSize;
    const uint8_t* pBlock = &self->allocationBitmap[idxOfAllocBitmapBlockModified * kSFSBlockSize];
    const LogicalBlockAddress allocationBitmapBlockLba = self->allocationBitmapLba + idxOfAllocBitmapBlockModified;

    memset(self->tmpBlock, 0, kSFSBlockSize);
    memcpy(self->tmpBlock, pBlock, &self->allocationBitmap[self->allocationBitmapByteSize] - pBlock);
    return DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, allocationBitmapBlockLba);
}

errno_t SerenaFS_AllocateBlock(SerenaFSRef _Nonnull self, LogicalBlockAddress* _Nonnull pOutLba)
{
    decl_try_err();
    LogicalBlockAddress lba = 0;    // Safe because LBA #0 is the volume header which is always allocated when the FS is mounted

    Lock_Lock(&self->allocationLock);

    for (LogicalBlockAddress i = 1; i < self->volumeBlockCount; i++) {
        if (!AllocationBitmap_IsBlockInUse(self->allocationBitmap, i)) {
            lba = i;
            break;
        }
    }
    if (lba == 0) {
        throw(ENOSPC);
    }

    AllocationBitmap_SetBlockInUse(self->allocationBitmap, lba, true);
    try(SerenaFS_WriteBackAllocationBitmapForLba(self, lba));
    Lock_Unlock(&self->allocationLock);

    *pOutLba = lba;
    return EOK;

catch:
    if (lba > 0) {
        AllocationBitmap_SetBlockInUse(self->allocationBitmap, lba, false);
    }
    Lock_Unlock(&self->allocationLock);
    *pOutLba = 0;
    return err;
}

void SerenaFS_DeallocateBlock(SerenaFSRef _Nonnull self, LogicalBlockAddress lba)
{
    if (lba == 0) {
        return;
    }

    Lock_Lock(&self->allocationLock);
    AllocationBitmap_SetBlockInUse(self->allocationBitmap, lba, false);

    // XXX check for error here?
    SerenaFS_WriteBackAllocationBitmapForLba(self, lba);
    Lock_Unlock(&self->allocationLock);
}
