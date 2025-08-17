//
//  RomDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RomDisk.h"
#include <kern/kalloc.h>
#include <kern/string.h>


#define MAX_NAME_LENGTH 8

final_class_ivars(RomDisk, DiskDriver,
    const char* _Nonnull    diskImage;
    scnt_t                  sectorCount;
    size_t                  sectorShift;
    size_t                  sectorSize;
    bool                    freeDiskImageOnClose;
    char                    name[MAX_NAME_LENGTH + 1];
);

IOCATS_DEF(g_cats, IODISK_ROMDISK);


errno_t RomDisk_Create(CatalogId parentDirId, const char* _Nonnull name, const void* _Nonnull pImage, size_t sectorSize, scnt_t sectorCount, bool freeOnClose, RomDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RomDiskRef self = NULL;

    if (pImage == NULL || !siz_ispow2(sectorSize)) {
        throw(EINVAL);
    }

    drive_info_t drvi;
    drvi.family = kDriveFamily_ROM;
    drvi.platter = kPlatter_None;
    drvi.properties = kDrive_IsReadOnly | kDrive_Fixed;

    try(DiskDriver_Create(class(RomDisk), g_cats, 0, parentDirId, &drvi, (DriverRef*)&self));
    self->diskImage = pImage;
    self->sectorCount = sectorCount;
    self->sectorShift = siz_log2(sectorSize);
    self->sectorSize = sectorSize;
    self->freeDiskImageOnClose = freeOnClose;
    String_CopyUpTo(self->name, name, MAX_NAME_LENGTH);

catch:
    *pOutSelf = self;
    return err;
}

void RomDisk_deinit(RomDiskRef _Nonnull self)
{
    if (self->freeDiskImageOnClose) {
        kfree(self->diskImage);
        self->diskImage = NULL;
    }
}


errno_t RomDisk_onStart(RomDiskRef _Nonnull _Locked self)
{
    SensedDisk info;
    info.sectorsPerTrack = self->sectorCount;
    info.heads = 1;
    info.cylinders = 1;
    info.sectorSize = self->sectorSize;
    info.sectorsPerRdwr = 1;
    info.properties = kDisk_IsReadOnly;
    DiskDriver_NoteSensedDisk((DiskDriverRef)self, &info);


    DriverEntry de;
    de.dirId = Driver_GetParentDirectoryId(self);
    de.name = self->name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0444);
    de.category = 0;
    de.driver = (HandlerRef)self;
    de.arg = 0;

    return Driver_Publish(self, &de);
}

void RomDisk_onStop(DriverRef _Nonnull _Locked self)
{
    Driver_Unpublish(self);
}

errno_t RomDisk_getSector(RomDiskRef _Nonnull self, const chs_t* _Nonnull chs, uint8_t* _Nonnull data, size_t secSize)
{
    memcpy(data, self->diskImage + (chs->s << self->sectorShift), secSize);
    return EOK;
}


class_func_defs(RomDisk, DiskDriver,
override_func_def(deinit, RomDisk, Object)
override_func_def(onStart, RomDisk, Driver)
override_func_def(onStop, RomDisk, Driver)
override_func_def(getSector, RomDisk, DiskDriver)
);
