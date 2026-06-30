//
//  vdm.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "vdm.h"
#include <driver/PlatformController.h>
#include <driver/disk/RamDisk.h>
#include <driver/disk/RomDisk.h>
#include <kpi/fd.h>


errno_t vdm_create_disk(int type, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nullable image)
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
        const fd_flags_t oflags = O_WRONLY;

        err = Driver_Open(dp, oflags);
        if (err == EOK) {
            err = DiskDriver_Write(dp, oflags, 0ll, image, sectorSize * sectorCount, &nBytesWritten);
            if (err == EOK && nBytesWritten != nBytesToWrite) {
                err = EIO;
            }

            Driver_Close(dp);
        }
        throw_iferr(err);
    }

    try(Driver_Launch((DriverRef)dp, (DriverRef)gPlatformController));
    

catch:
    Object_Release(dp);
    return err;
}
