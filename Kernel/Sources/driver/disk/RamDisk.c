//
//  RamDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RamDisk.h"
#include <string.h>
#include <kern/kalloc.h>


typedef struct DiskExtent {
    SListNode   node;
    blkno_t     firstSectorIndex;
    char        data[1];
} DiskExtent;


#define MAX_NAME_LENGTH 8

// All ivars are protected by the dispatch queue
final_class_ivars(RamDisk, DiskDriver,
    SList       extents;            // Sorted ascending by 'firstSectorIndex'
    blkcnt_t    extentSectorCount;  // How many blocks an extent stores
    scnt_t      sectorCount;
    size_t      sectorShift;
    size_t      sectorSize;
    char        fillByte;
    char        name[MAX_NAME_LENGTH];
);

IOCATS_DEF(g_cats, IODISK_RAMDISK);


errno_t RamDisk_Create(const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, scnt_t extentSectorCount, RamDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RamDiskRef self = NULL;

    if (!siz_ispow2(sectorSize)) {
        throw(EINVAL);
    }

    drive_info_t drvi;
    drvi.platter = kPlatter_None;
    drvi.properties = kDrive_Fixed;

    try(DiskDriver_Create(class(RamDisk), 0, g_cats, &drvi, (DriverRef*)&self));
    self->extentSectorCount = __min(extentSectorCount, sectorCount);
    self->sectorCount = sectorCount;
    self->sectorShift = siz_log2(sectorSize);
    self->sectorSize = sectorSize;
    self->fillByte = 0;
    strncpy(self->name, name, MAX_NAME_LENGTH);

catch:
    *pOutSelf = self;
    return err;
}

void RamDisk_deinit(RamDiskRef _Nonnull self)
{
    SList_ForEach(&self->extents, DiskExtent, {
        kfree(pCurNode);
    });
}

errno_t RamDisk_onStart(RamDiskRef _Nonnull self)
{
    SensedDisk info;
    info.sectorsPerTrack = self->sectorCount;
    info.heads = 1;
    info.cylinders = 1;
    info.sectorSize = self->sectorSize;
    info.sectorsPerRdwr = 1;
    info.properties = 0;
    DiskDriver_NoteSensedDisk((DiskDriverRef)self, &info);


    DriverEntry de;
    de.name = self->name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.arg = 0;

    return Driver_Publish((DriverRef)self, &de);
}

// Tries to find the disk extent that contains the given sector index. This disk
// extent is returned if it exists. Also returns the disk extent that exists and
// is closest to the given sector index and whose 'firstSectorIndex' is <= the
// given sector index. 
static DiskExtent* _Nullable RamDisk_GetDiskExtentForSectorIndex_Locked(RamDiskRef _Nonnull self, blkno_t lba, DiskExtent* _Nullable * _Nullable pOutDiskExtentBeforeSectorIndex)
{
    DiskExtent* pPrevExtent = NULL;
    DiskExtent* pExtent = NULL;
    const blkcnt_t extentSectorCount = self->extentSectorCount;

    SList_ForEach(&self->extents, DiskExtent, {
        const blkno_t firstSectorIndex = pCurNode->firstSectorIndex;

        if (lba >= firstSectorIndex && lba < (firstSectorIndex + extentSectorCount)) {
            pExtent = pCurNode;
            break;
        }
        else if (lba < firstSectorIndex) {
            break;
        }

        pPrevExtent = pCurNode;
    });

    if (pOutDiskExtentBeforeSectorIndex) {
        *pOutDiskExtentBeforeSectorIndex = pPrevExtent;
    }
    return pExtent;
}

errno_t RamDisk_getSector(RamDiskRef _Nonnull self, const chs_t* _Nonnull chs, uint8_t* _Nonnull data, size_t secSize)
{
    DiskExtent* pExtent = RamDisk_GetDiskExtentForSectorIndex_Locked(self, chs->s, NULL);

    if (pExtent) {
        // Request for a sector that was previously written to -> return the sector
        memcpy(data, &pExtent->data[(chs->s - pExtent->firstSectorIndex) << self->sectorShift], secSize);
    }
    else {
        // Request for a sector that hasn't been written to yet -> return zeros
        memset(data, self->fillByte, secSize);
    }

    return EOK;
}

// Adds a new extent after 'pPrevExtent' and before 'pPrevExtent'->next. All data
// in the newly allocated extent is cleared. 'firstSectorIndex' is the index of the
// first sector in the newly allocated extent. Remember that we allocate extents
// on demand which means that the end of 'pPrevExtent' is not necessarily the
// beginning of the new extent in terms of sector numbers.
static errno_t RamDisk_AddExtentAfter_Locked(RamDiskRef _Nonnull self, blkno_t firstSectorIndex, DiskExtent* _Nullable pPrevExtent, DiskExtent* _Nullable * _Nonnull pOutExtent)
{
    decl_try_err();
    DiskExtent* pExtent = NULL;

    try(kalloc_cleared(sizeof(DiskExtent) - 1 + (self->extentSectorCount << self->sectorShift), (void**)&pExtent));
    SList_InsertAfter(&self->extents, &pExtent->node, (pPrevExtent) ? &pPrevExtent->node : NULL);
    pExtent->firstSectorIndex = firstSectorIndex;

catch:
    *pOutExtent = pExtent;
    return err;
}

errno_t RamDisk_putSector(RamDiskRef _Nonnull self, const chs_t* _Nonnull chs, const uint8_t* _Nonnull data, size_t secSize)
{
    decl_try_err();

    DiskExtent* pPrevExtent;
    DiskExtent* pExtent = RamDisk_GetDiskExtentForSectorIndex_Locked(self, chs->s, &pPrevExtent);
    
    if (pExtent == NULL) {
        // Extent doesn't exist yet for the range intersected by 'ba'. Allocate
        // it and make sure all the data in there is cleared out.
        err = RamDisk_AddExtentAfter_Locked(self, (chs->s / self->extentSectorCount) * self->extentSectorCount, pPrevExtent, &pExtent);
    }

    if (pExtent) {
        memcpy(&pExtent->data[(chs->s - pExtent->firstSectorIndex) << self->sectorShift], data, secSize);
    }

    return err;
}


errno_t RamDisk_doFormatDisk(RamDiskRef _Nonnull self, char fillByte)
{
    SList_ForEach(&self->extents, DiskExtent, {
        kfree(pCurNode);
    });

    self->extents = SLIST_INIT;
    self->fillByte = fillByte;

    return EOK;
}


class_func_defs(RamDisk, DiskDriver,
override_func_def(deinit, RamDisk, Object)
override_func_def(onStart, RamDisk, Driver)
override_func_def(getSector, RamDisk, DiskDriver)
override_func_def(putSector, RamDisk, DiskDriver)
override_func_def(doFormatDisk, RamDisk, DiskDriver)
);
