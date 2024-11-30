//
//  startup_bootfs.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <krt/krt.h>
#include <klib/klib.h>
#include <dispatcher/VirtualProcessor.h>
#include <driver/DriverCatalog.h>
#include <driver/amiga/floppy/FloppyDriver.h>
#include <filesystem/DiskFSContainer.h>
#include <filesystem/serenafs/SerenaFS.h>

#define MAX_NAME_LENGTH 16


// Finds a RAM or ROM disk to boot from. Returns kDriver_None if drive found.
static DriverId get_boot_mem_disk_id(void)
{
    DriverId driverId = DriverCatalog_GetDriverIdForName(gDriverCatalog, "ram");

    if (driverId == kDriverId_None) {
        driverId = DriverCatalog_GetDriverIdForName(gDriverCatalog, "rom");
    }

    return driverId;
}

// Finds a floppy disk to boot from. Returns kDriver_None if no bootable floppy
// drive found.
static DriverId get_boot_floppy_disk_id(void)
{
    return DriverCatalog_GetDriverIdForName(gDriverCatalog, kFloppyDrive0Name);
}


// Tries to mount the root filesystem from a floppy disk in drive 0.
static errno_t boot_from_disk(DriverId diskId, bool shouldRetry, FilesystemRef _Nullable * _Nonnull pOutFS)
{
    decl_try_err();
    errno_t lastError = EOK;
    DiskDriverRef driver;
    DiskInfo info;
    FSContainerRef fsContainer;
    FilesystemRef fs;
    bool shouldPromptForDisk = true;

    try_null(driver, (DiskDriverRef) DriverCatalog_CopyDriverForDriverId(gDriverCatalog, diskId), ENODEV);
    try(DiskDriver_GetInfo(driver, &info));
    try(DiskFSContainer_Create(diskId, info.mediaId, &fsContainer));
    try(SerenaFS_Create(fsContainer, (SerenaFSRef*)&fs));

    while (true) {
        err = Filesystem_Start(fs, NULL, 0);

        if (err == EOK) {
            break;
        }
        else if (err == EDISKCHANGE) {
            // This means that the user inserted a new disk and that the disk
            // hardware isn't able to automatically pick this change up on its
            // own. Just try mounting again. 2nd time around should work.
            lastError = err;
            continue;
        }
        else if (err != ENOMEDIUM && err != lastError) {
            print("Error: %d\n\n", err);
            lastError = err;
            shouldPromptForDisk = true;
        }

        if (!shouldRetry) {
            // No disk or no mountable disk. We have a fallback though so bail
            // out and let the caller try another option.
            return err;
        }

        if (shouldPromptForDisk) {
            print("Please insert a Serena boot disk...\n\n");
            shouldPromptForDisk = false;
        }

        VirtualProcessor_Sleep(TimeInterval_MakeSeconds(1));
    }

    char buf[MAX_NAME_LENGTH+1];
    DriverCatalog_CopyNameForDriverId(gDriverCatalog, diskId, buf, MAX_NAME_LENGTH);
    print("Booting from ");
    print(buf);
    print("...\n\n");

    Object_Release(driver);
    *pOutFS = fs;
    return EOK;

catch:
    Object_Release(driver);
    Object_Release(fs);
    *pOutFS = NULL;
    return err;
}

// Locates the root filesystem and mounts it.
FilesystemRef _Nonnull create_boot_filesystem(void)
{
    decl_try_err();
    int state = 0;
    FilesystemRef fs = NULL;
    const DriverId memDiskId = get_boot_mem_disk_id();
    DriverId diskId = kDriverId_None;
    bool shouldRetry = false;

    while (true) {
        switch (state) {
            case 0:
                // Boot floppy disk
                diskId = get_boot_floppy_disk_id();
                shouldRetry = (memDiskId == kDriverId_None);
                break;

            case 1:
                // ROM disk image, if it exists
                diskId = memDiskId;
                shouldRetry = false;
                break;

            default:
                state = -1;
                break;
        }
        if (state < 0) {
            break;
        }

        if (diskId != kDriverId_None) {
            err = boot_from_disk(diskId, shouldRetry, &fs);
            if (err == EOK) {
                return fs;
            }
        }

        state++;
    }


    // No luck, give up
    print("No boot device found.\nHalting...\n");
    while(true);
    /* NOT REACHED */
}
