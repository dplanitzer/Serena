//
//  RamDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RamDisk.h"
#include "Lock.h"
#include <System/IOChannel.h>


typedef struct DiskExtent {
    SListNode   node;
    size_t      firstBlockIndex;
    char        data[1];
} DiskExtent;


CLASS_IVARS(RamDisk, DiskDriver,
    SList       extents;            // Sorted ascending by 'firstBlockIndex'
    size_t      extentBlockCount;   // How many blocks an extent stores
    size_t      blockCount;
    size_t      blockSize;
    Lock        lock;               // Protects extent allocation and lookup
);


errno_t RamDisk_Create(size_t nBlockCount, size_t nBlockSize, size_t nExtentBlockCount, RamDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RamDiskRef self;

    try(Object_Create(RamDisk, &self));
    SList_Init(&self->extents);
    self->extentBlockCount = __min(nExtentBlockCount, nBlockCount);
    self->blockCount = nBlockCount;
    self->blockSize = nBlockSize;
    Lock_Init(&self->lock);

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void RamDisk_deinit(RamDiskRef _Nonnull self)
{
    SList_ForEach(&self->extents, DiskExtent, {
        kfree(pCurNode);
    });

    SList_Deinit(&self->extents);
}

// Returns the size of a block.
size_t RamDisk_getBlockSize(RamDiskRef _Nonnull self)
{
    return self->blockSize;
}

// Returns the number of blocks that the disk is able to store.
size_t RamDisk_getBlockCount(RamDiskRef _Nonnull self)
{
    return self->blockCount;
}

// Returns true if the disk if read-only.
bool RamDisk_isReadOnly(RamDiskRef _Nonnull self)
{
    return false;
}

// Tries to find the disk extent that contains the given block index. This disk
// extent is returned if it exists. Also returns the disk extent that exists and
// is closest to the given block index and whose 'firstBlockIndex' is <= the
// given block index. 
static DiskExtent* _Nullable RamDisk_GetDiskExtentForBlockIndex_Locked(RamDiskRef _Nonnull self, size_t idx, DiskExtent* _Nullable * _Nullable pOutDiskExtentBeforeBlockIndex)
{
    DiskExtent* pPrevExtent = NULL;
    DiskExtent* pExtent = NULL;
    const size_t extentBlockCount = self->extentBlockCount;

    SList_ForEach(&self->extents, DiskExtent, {
        const size_t firstBlockIndex = pCurNode->firstBlockIndex;

        if (idx >= firstBlockIndex && idx < (firstBlockIndex + extentBlockCount)) {
            pExtent = pCurNode;
            break;
        }
        else if (idx < firstBlockIndex) {
            break;
        }

        pPrevExtent = pCurNode;
    });

    if (pOutDiskExtentBeforeBlockIndex) {
        *pOutDiskExtentBeforeBlockIndex = pPrevExtent;
    }
    return pExtent;
}

// Reads the contents of the block at index 'idx'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t RamDisk_getBlock(RamDiskRef _Nonnull self, void* _Nonnull pBuffer, size_t idx)
{
    if (idx >= self->blockCount) {
        return EIO;
    }


    Lock_Lock(&self->lock);

    DiskExtent* pExtent = RamDisk_GetDiskExtentForBlockIndex_Locked(self, idx, NULL);
    if (pExtent) {
        // Request for a block that was previously written to -> return the block
        Bytes_CopyRange(pBuffer, &pExtent->data[(idx - pExtent->firstBlockIndex) * self->blockSize], self->blockSize);
    }
    else {
        // Request for a block that hasn't been written to yet -> return zeros
        Bytes_ClearRange(pBuffer, self->blockSize);
    }

    Lock_Unlock(&self->lock);

    return EOK;
}

// Adds a new extent after 'pPrevExtent' and before 'pPrevExtent'->next. All data
// in the newly allocated extent is cleared. 'firstBlockIndex' is the index of the
// first block in the newly allocated extent. Remember that we allocate extents
// on demand which means that the end of 'pPrevExtent' is not necessarily the
// beginning of the new extent in terms of block numbers.
static errno_t RamDisk_AddExtentAfter_Locked(RamDiskRef _Nonnull self, size_t firstBlockIndex, DiskExtent* _Nullable pPrevExtent, DiskExtent* _Nullable * _Nonnull pOutExtent)
{
    decl_try_err();
    DiskExtent* pExtent;

    try(kalloc_cleared(sizeof(DiskExtent) - 1 + self->extentBlockCount * self->blockSize, (void**)&pExtent));
    SList_InsertAfter(&self->extents, &pExtent->node, (pPrevExtent) ? &pPrevExtent->node : NULL);
    pExtent->firstBlockIndex = firstBlockIndex;
    *pOutExtent = pExtent;

catch:
    return err;
}

// Writes the contents of 'pBuffer' to the block at index 'idx'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
errno_t RamDisk_putBlock(RamDiskRef _Nonnull self, const void* _Nonnull pBuffer, size_t idx)
{
    decl_try_err();

    if (idx >= self->blockCount) {
        return EIO;
    }

    Lock_Lock(&self->lock);

    DiskExtent* pPrevExtent;
    DiskExtent* pExtent = RamDisk_GetDiskExtentForBlockIndex_Locked(self, idx, &pPrevExtent);
    if (pExtent == NULL) {
        // Extent doesn't exist yet for the range intersected by 'idx'. Allocate
        // it and make sure all the data in there is cleared out.
        try(RamDisk_AddExtentAfter_Locked(self, (idx / self->extentBlockCount) * self->extentBlockCount, pPrevExtent, &pExtent));
    }
    if (pExtent) {
        Bytes_CopyRange(&pExtent->data[(idx - pExtent->firstBlockIndex) * self->blockSize], pBuffer, self->blockSize);
    }

catch:
    Lock_Unlock(&self->lock);

    return err;
}


CLASS_METHODS(RamDisk, DiskDriver,
OVERRIDE_METHOD_IMPL(deinit, RamDisk, Object)
OVERRIDE_METHOD_IMPL(getBlockSize, RamDisk, DiskDriver)
OVERRIDE_METHOD_IMPL(getBlockCount, RamDisk, DiskDriver)
OVERRIDE_METHOD_IMPL(isReadOnly, RamDisk, DiskDriver)
OVERRIDE_METHOD_IMPL(getBlock, RamDisk, DiskDriver)
OVERRIDE_METHOD_IMPL(putBlock, RamDisk, DiskDriver)
);
