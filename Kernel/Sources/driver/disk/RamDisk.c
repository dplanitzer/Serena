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


#define MAX_NAME_LENGTH 8

// All ivars are protected by the dispatch queue
final_class_ivars(RamDisk, DiskDriver,
    SList               extents;            // Sorted ascending by 'firstBlockIndex'
    LogicalBlockCount   extentBlockCount;   // How many blocks an extent stores
    LogicalBlockCount   blockCount;
    size_t              blockSize;
    size_t              blockShift;
    char                name[MAX_NAME_LENGTH];
);


errno_t RamDisk_Create(DriverRef _Nullable parent, const char* _Nonnull name, size_t blockSize, LogicalBlockCount blockCount, LogicalBlockCount extentBlockCount, RamDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RamDiskRef self = NULL;

    if (!siz_ispow2(blockSize)) {
        throw(EINVAL);
    }

    try(DiskDriver_Create(class(RamDisk), 0, parent, (DriverRef*)&self));
    SList_Init(&self->extents);
    self->extentBlockCount = __min(extentBlockCount, blockCount);
    self->blockCount = blockCount;
    self->blockSize = blockSize;
    self->blockShift = siz_log2(self->blockSize);
    String_CopyUpTo(self->name, name, MAX_NAME_LENGTH);

    MediaInfo info;
    info.blockCount = self->blockCount;
    info.blockSize = self->blockSize;
    info.isReadOnly = false;
    DiskDriver_NoteMediaLoaded((DiskDriverRef)self, &info);

catch:
    *pOutSelf = self;
    return err;
}

void RamDisk_deinit(RamDiskRef _Nonnull self)
{
    SList_ForEach(&self->extents, DiskExtent, {
        kfree(pCurNode);
    });

    SList_Deinit(&self->extents);
}

errno_t RamDisk_onStart(RamDiskRef _Nonnull self)
{
    DriverEntry de;
    de.name = self->name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = FilePermissions_MakeFromOctal(0666);
    de.arg = 0;

    return Driver_Publish((DriverRef)self, &de);
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

errno_t RamDisk_getBlock(RamDiskRef _Nonnull self, const IORequest* _Nonnull ior)
{
    const LogicalBlockAddress lba = ior->address.lba;
    void* dp = DiskBlock_GetMutableData(ior->block);

    if (lba >= self->blockCount) {
        return ENXIO;
    }


    DiskExtent* pExtent = RamDisk_GetDiskExtentForBlockIndex_Locked(self, lba, NULL);
    if (pExtent) {
        // Request for a block that was previously written to -> return the block
        memcpy(dp, &pExtent->data[(lba - pExtent->firstBlockIndex) << self->blockShift], self->blockSize);
    }
    else {
        // Request for a block that hasn't been written to yet -> return zeros
        memset(dp, 0, self->blockSize);
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

    try(kalloc_cleared(sizeof(DiskExtent) - 1 + (self->extentBlockCount << self->blockShift), (void**)&pExtent));
    SList_InsertAfter(&self->extents, &pExtent->node, (pPrevExtent) ? &pPrevExtent->node : NULL);
    pExtent->firstBlockIndex = firstBlockIndex;
    *pOutExtent = pExtent;

catch:
    return err;
}

errno_t RamDisk_putBlock(RamDiskRef _Nonnull self, const IORequest* _Nonnull ior)
{
    decl_try_err();
    const LogicalBlockAddress lba = ior->address.lba;
    const void* sp = DiskBlock_GetData(ior->block);

    if (lba >= self->blockCount) {
        return ENXIO;
    }

    DiskExtent* pPrevExtent;
    DiskExtent* pExtent = RamDisk_GetDiskExtentForBlockIndex_Locked(self, lba, &pPrevExtent);
    if (pExtent == NULL) {
        // Extent doesn't exist yet for the range intersected by 'lba'. Allocate
        // it and make sure all the data in there is cleared out.
        try(RamDisk_AddExtentAfter_Locked(self, (lba / self->extentBlockCount) * self->extentBlockCount, pPrevExtent, &pExtent));
    }
    if (pExtent) {
        memcpy(&pExtent->data[(lba - pExtent->firstBlockIndex) << self->blockShift], sp, self->blockSize);
    }

catch:
    return err;
}


class_func_defs(RamDisk, DiskDriver,
override_func_def(deinit, RamDisk, Object)
override_func_def(onStart, RamDisk, Driver)
override_func_def(getBlock, RamDisk, DiskDriver)
override_func_def(putBlock, RamDisk, DiskDriver)
);
