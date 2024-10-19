//
//  RamDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RamDisk.h"


typedef struct DiskExtent {
    SListNode           node;
    LogicalBlockAddress firstBlockIndex;
    char                data[1];
} DiskExtent;


// All ivars are protected by the dispatch queue
final_class_ivars(RamDisk, DiskDriver,
    SList               extents;            // Sorted ascending by 'firstBlockIndex'
    LogicalBlockCount   extentBlockCount;   // How many blocks an extent stores
    LogicalBlockCount   blockCount;
    size_t              blockSize;
);


errno_t RamDisk_Create(size_t nBlockSize, LogicalBlockCount nBlockCount, LogicalBlockCount nExtentBlockCount, RamDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RamDiskRef self;

    try(DiskDriver_Create(RamDisk, &self));
    SList_Init(&self->extents);
    self->extentBlockCount = __min(nExtentBlockCount, nBlockCount);
    self->blockCount = nBlockCount;
    self->blockSize = nBlockSize;

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

// Returns information about the disk drive and the media loaded into the
// drive.
errno_t RamDisk_getInfoAsync(RamDiskRef _Nonnull self, DiskInfo* pOutInfo)
{
    pOutInfo->blockSize = self->blockSize;
    pOutInfo->blockCount = self->blockCount;
    pOutInfo->mediaId = 1;
    pOutInfo->isReadOnly = false;

    return EOK;
}

// Tries to find the disk extent that contains the given block index. This disk
// extent is returned if it exists. Also returns the disk extent that exists and
// is closest to the given block index and whose 'firstBlockIndex' is <= the
// given block index. 
static DiskExtent* _Nullable RamDisk_GetDiskExtentForBlockIndex_Locked(RamDiskRef _Nonnull self, LogicalBlockAddress lba, DiskExtent* _Nullable * _Nullable pOutDiskExtentBeforeBlockIndex)
{
    DiskExtent* pPrevExtent = NULL;
    DiskExtent* pExtent = NULL;
    const LogicalBlockCount extentBlockCount = self->extentBlockCount;

    SList_ForEach(&self->extents, DiskExtent, {
        const LogicalBlockAddress firstBlockIndex = pCurNode->firstBlockIndex;

        if (lba >= firstBlockIndex && lba < (firstBlockIndex + extentBlockCount)) {
            pExtent = pCurNode;
            break;
        }
        else if (lba < firstBlockIndex) {
            break;
        }

        pPrevExtent = pCurNode;
    });

    if (pOutDiskExtentBeforeBlockIndex) {
        *pOutDiskExtentBeforeBlockIndex = pPrevExtent;
    }
    return pExtent;
}

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t RamDisk_getBlockAsync(RamDiskRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    if (lba >= self->blockCount) {
        return EIO;
    }


    DiskExtent* pExtent = RamDisk_GetDiskExtentForBlockIndex_Locked(self, lba, NULL);
    if (pExtent) {
        // Request for a block that was previously written to -> return the block
        memcpy(pBuffer, &pExtent->data[(lba - pExtent->firstBlockIndex) * self->blockSize], self->blockSize);
    }
    else {
        // Request for a block that hasn't been written to yet -> return zeros
        memset(pBuffer, 0, self->blockSize);
    }

    return EOK;
}

// Adds a new extent after 'pPrevExtent' and before 'pPrevExtent'->next. All data
// in the newly allocated extent is cleared. 'firstBlockIndex' is the index of the
// first block in the newly allocated extent. Remember that we allocate extents
// on demand which means that the end of 'pPrevExtent' is not necessarily the
// beginning of the new extent in terms of block numbers.
static errno_t RamDisk_AddExtentAfter_Locked(RamDiskRef _Nonnull self, LogicalBlockAddress firstBlockIndex, DiskExtent* _Nullable pPrevExtent, DiskExtent* _Nullable * _Nonnull pOutExtent)
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

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
errno_t RamDisk_putBlockAsync(RamDiskRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    decl_try_err();

    if (lba >= self->blockCount) {
        return EIO;
    }

    DiskExtent* pPrevExtent;
    DiskExtent* pExtent = RamDisk_GetDiskExtentForBlockIndex_Locked(self, lba, &pPrevExtent);
    if (pExtent == NULL) {
        // Extent doesn't exist yet for the range intersected by 'lba'. Allocate
        // it and make sure all the data in there is cleared out.
        try(RamDisk_AddExtentAfter_Locked(self, (lba / self->extentBlockCount) * self->extentBlockCount, pPrevExtent, &pExtent));
    }
    if (pExtent) {
        memcpy(&pExtent->data[(lba - pExtent->firstBlockIndex) * self->blockSize], pBuffer, self->blockSize);
    }

catch:
    return err;
}


class_func_defs(RamDisk, DiskDriver,
override_func_def(deinit, RamDisk, Object)
override_func_def(getInfoAsync, RamDisk, DiskDriver)
override_func_def(getBlockAsync, RamDisk, DiskDriver)
override_func_def(putBlockAsync, RamDisk, DiskDriver)
);
