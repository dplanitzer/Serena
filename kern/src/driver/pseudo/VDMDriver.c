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
#include <kpi/fd.h>

IOCATS_DEF(g_cats, IOSRV_VDM);


final_class_ivars(VDMDriver, Driver,
);


errno_t VDMDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    VDMDriverRef self;

    err = Driver_CreateRoot(class(VDMDriver), kDriver_IsBus, g_cats, (DriverRef*)&self);
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

errno_t VDMDriver_CreateDisk(VDMDriverRef _Nonnull self, int type, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nullable image)
{
    decl_try_err();
    DriverRef dp = NULL;
    ssize_t nBytesToWrite = 0, nBytesWritten;

    if (sectorSize == 0 || sectorCount == 0) {
        throw(EIO);
    }

    switch (type) {
        case VDM_TYPE_RAM:
            err = RamDisk_Create(name, sectorSize, sectorCount, 128, (RamDiskRef*)&dp);
            nBytesToWrite = sectorSize * sectorCount;
            break;

        case VDM_TYPE_REF_ROM:
            err = RomDisk_Create(name, image, sectorSize, sectorCount, false, (RomDiskRef*)&dp);
            break;

        default:
            err = EINVAL;
            break;
    }
    throw_iferr(err);

    
    if (nBytesToWrite > 0 && image) {
        unsigned int mode = O_WRONLY;

        err = Driver_Open(dp, mode, 0, NULL);
        if (err == EOK) {
            err = Driver_Write(dp, mode, 0ll, image, sectorSize * sectorCount, &nBytesWritten);
            if (err == EOK && nBytesWritten != nBytesToWrite) {
                err = EIO;
            }

            Driver_Close(dp);
        }
        throw_iferr(err);
    }

    try(Driver_AttachStartChild((DriverRef)self, dp, (size_t)-1));
    

catch:
    Object_Release(dp);
    return err;
}


class_func_defs(VDMDriver, Driver,
override_func_def(onStart, VDMDriver, Driver)
);
