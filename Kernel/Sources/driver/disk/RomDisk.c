//
//  RomDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RomDisk.h"


#define MAX_NAME_LENGTH 8

final_class_ivars(RomDisk, DiskDriver,
    const char* _Nonnull    diskImage;
    size_t                  sectorShift;
    bool                    freeDiskImageOnClose;
    char                    name[MAX_NAME_LENGTH + 1];
);


errno_t RomDisk_Create(DriverRef _Nullable parent, const char* _Nonnull name, const void* _Nonnull pImage, size_t sectorSize, LogicalBlockCount sectorCount, bool freeOnClose, RomDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RomDiskRef self = NULL;

    if (pImage == NULL || !siz_ispow2(sectorSize)) {
        throw(EINVAL);
    }

    MediaInfo info;
    info.sectorsPerTrack = sectorCount;
    info.heads = 1;
    info.cylinders = 1;
    info.sectorSize = sectorSize;
    info.formatSectorCount = 0;
    info.properties = kMediaProperty_IsReadOnly;

    try(DiskDriver_Create(class(RomDisk), 0, parent, &info, (DriverRef*)&self));
    self->diskImage = pImage;
    self->sectorShift = siz_log2(sectorSize);
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
    DriverEntry de;
    de.name = self->name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = FilePermissions_MakeFromOctal(0444);
    de.arg = 0;

    return Driver_Publish((DriverRef)self, &de);
}

errno_t RomDisk_getSector(RomDiskRef _Nonnull self, const chs_t* _Nonnull chs, uint8_t* _Nonnull data, size_t secSize)
{
    memcpy(data, self->diskImage + (chs->s << self->sectorShift), secSize);
    return EOK;
}


class_func_defs(RomDisk, DiskDriver,
override_func_def(deinit, RomDisk, Object)
override_func_def(onStart, RomDisk, Driver)
override_func_def(getSector, RomDisk, DiskDriver)
);
