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
#include <filesystem/IOChannel.h>


final_class_ivars(VDMDriver, PseudoDriver,
);


errno_t VDMDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    VDMDriverRef self;

    err = PseudoDriver_Create(class(VDMDriver), kDriver_IsBus, (DriverRef*)&self);
    if (err == EOK) {
        Driver_SetMaxChildCount((DriverRef)self, 8);
    }

    *pOutSelf = (DriverRef)self;
    return err;
}

errno_t VDMDriver_onStart(VDMDriverRef _Nonnull _Locked self)
{
    decl_try_err();

    DirEntry be;
    be.name = "vd-bus";
    be.uid = UID_ROOT;
    be.gid = GID_ROOT;
    be.perms = fs_perms_from_octal(0755);

    DriverEntry de;
    de.name = "self";
    de.uid = UID_ROOT;
    de.gid = GID_ROOT;
    de.perms = fs_perms_from_octal(0666);
    de.arg = 0;

    try(Driver_PublishBus((DriverRef)self, &be, &de));

catch:
    return err;
}

errno_t VDMDriver_CreateRamDisk(VDMDriverRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nullable image)
{
    decl_try_err();
    DriverRef dp = NULL;
    IOChannelRef ch = NULL;
    ssize_t nBytesWritten;

    if (sectorSize == 0 || sectorCount == 0) {
        throw(EIO);
    }

    try(RamDisk_Create(name, sectorSize, sectorCount, 128, (RamDiskRef*)&dp));

    if (image) {
        const ssize_t nBytesToWrite = sectorSize * sectorCount;

        try(Driver_Open(dp, O_WRONLY, 0, &ch));
        try(IOChannel_Write(ch, image, sectorSize * sectorCount, &nBytesWritten));
        if (nBytesWritten != nBytesToWrite) {
            throw(EIO);
        }
    }

    try(Driver_AttachStartChild((DriverRef)self, dp, (size_t)-1));
    
catch:
    IOChannel_Release(ch);
    Object_Release(dp);
    return err;
}

errno_t VDMDriver_CreateRomDisk(VDMDriverRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nonnull image)
{
    decl_try_err();
    DriverRef dp = NULL;

    if (sectorSize == 0 || sectorCount == 0) {
        throw(EIO);
    }

    try(RomDisk_Create(name, image, sectorSize, sectorCount, false, (RomDiskRef*)&dp));
    try(Driver_AttachStartChild((DriverRef)self, dp, (size_t)-1));

catch:
    Object_Release(dp);
    return err;
}


class_func_defs(VDMDriver, PseudoDriver,
override_func_def(onStart, VDMDriver, Driver)
);
