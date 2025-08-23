//
//  VDMDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "VDMDriver.h"
#include <driver/disk/RamDisk.h>
#include <driver/disk/RomDisk.h>
#include <sched/mtx.h>


final_class_ivars(VDMDriver, PseudoDriver,
    mtx_t   io_mtx;
);


errno_t VDMDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    VDMDriverRef self;

    err = PseudoDriver_Create(class(VDMDriver), kDriver_IsBus, (DriverRef*)&self);
    if (err == EOK) {
        mtx_init(&self->io_mtx);
        Driver_SetMaxChildCount((DriverRef)self, 8);
    }

    *pOutSelf = (DriverRef)self;
    return err;
}

errno_t VDMDriver_onStart(VDMDriverRef _Nonnull _Locked self)
{
    decl_try_err();

    DirEntry be;
    be.dirId = Driver_GetBusDirectory((DriverRef)self);
    be.name = "vd-bus";
    be.uid = kUserId_Root;
    be.gid = kGroupId_Root;
    be.perms = perm_from_octal(0755);

    try(Driver_PublishBusDirectory((DriverRef)self, &be));

    DriverEntry de;
    de.dirId = Driver_GetPublishedBusDirectory(self);
    de.name = "self";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.arg = 0;

    try(Driver_Publish(self, &de));

catch:
    return err;
}

static errno_t _attach_start_disk(VDMDriverRef _Nonnull self, DriverRef _Nonnull disk)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    const ssize_t slotId = Driver_GetFirstAvailableSlotId((DriverRef)self);

    if (slotId >= 0) {
        err = Driver_AttachStartChild((DriverRef)self, disk, slotId);
    }
    else {
        err = ENXIO;
    }
    mtx_unlock(&self->io_mtx);

    return err;
}

errno_t VDMDriver_CreateRamDisk(VDMDriverRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, scnt_t extentSectorCount)
{
    decl_try_err();
    DriverRef dp;

    try(RamDisk_Create(name, sectorSize, sectorCount, extentSectorCount, (RamDiskRef*)&dp));
    try(_attach_start_disk(self, dp));
    
catch:
    Object_Release(dp);
    return err;
}

errno_t VDMDriver_CreateRomDisk(VDMDriverRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nonnull image)
{
    decl_try_err();
    DriverRef dp;

    try(RomDisk_Create(name, image, sectorSize, sectorCount, false, (RomDiskRef*)&dp));
    try(_attach_start_disk(self, dp));

catch:
    Object_Release(dp);
    return err;
}


class_func_defs(VDMDriver, PseudoDriver,
override_func_def(onStart, VDMDriver, Driver)
);
