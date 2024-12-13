//
//  bootfs.c
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
#include <filesystem/FilesystemManager.h>

#define MAX_NAME_LENGTH 16


// Finds a RAM or ROM disk to boot from. Returns kDriver_None if drive found.
static DiskDriverRef _Nullable copy_boot_mem_driver(void)
{
    DriverRef drv = DriverCatalog_CopyDriverForName(gDriverCatalog, "ram");

    if (drv == NULL) {
        drv = DriverCatalog_CopyDriverForName(gDriverCatalog, "rom");
    }

    return (DiskDriverRef)drv;
}

// Finds a floppy disk to boot from. Returns kDriver_None if no bootable floppy
// drive found.
static DiskDriverRef _Nullable copy_boot_floppy_driver(void)
{
    return (DiskDriverRef)DriverCatalog_CopyDriverForName(gDriverCatalog, kFloppyDrive0Name);
}


// Tries to mount the root filesystem stored on the mass storage device
// represented by 'pDriver'.
static errno_t boot_from_disk(DiskDriverRef _Nonnull pDriver, bool shouldRetry, FilesystemRef _Nullable * _Nonnull pOutFS)
{
    decl_try_err();
    errno_t lastError = EOK;
    FilesystemRef fs;
    bool shouldPromptForDisk = true;

    while (true) {
        fs = NULL;
        err = FilesystemManager_DiscoverAndStartFilesystem(gFilesystemManager, pDriver, NULL, 0, &fs);

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
    DriverCatalog_CopyNameForDriverId(gDriverCatalog, Driver_GetDriverId(pDriver), buf, MAX_NAME_LENGTH);
    print("Booting from ");
    print(buf);
    print("...\n\n");

    *pOutFS = fs;
    return EOK;

catch:
    Object_Release(fs);
    *pOutFS = NULL;
    return err;
}

// Locates the boot device and creates the boot filesystem. Halts the machine if
// a boot device/filesystem can not be found.
FilesystemRef _Nullable create_boot_filesystem(void)
{
    decl_try_err();
    int state = 0;
    FilesystemRef fs = NULL;
    DiskDriverRef pMemDriver = copy_boot_mem_driver();
    DiskDriverRef pDriver = NULL;
    bool shouldRetry = false;

    while (true) {
        switch (state) {
            case 0:
                // Boot floppy disk
                pDriver = copy_boot_floppy_driver();
                shouldRetry = (pMemDriver == NULL) ? true : false;
                break;

            case 1:
                // ROM disk image, if it exists
                pDriver = Object_RetainAs(pMemDriver, DiskDriver);
                shouldRetry = false;
                break;

            default:
                state = -1;
                break;
        }
        if (state < 0) {
            break;
        }

        if (pDriver) {
            err = boot_from_disk(pDriver, shouldRetry, &fs);
            Object_Release(pDriver);

            if (err == EOK) {
                break;
            }
        }

        state++;
    }

    Object_Release(pMemDriver);
    return fs;
}
