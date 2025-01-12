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
#include <driver/disk/DiskDriver.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/IOChannel.h>


// Finds a RAM or ROM disk to boot from and returns the in-kernel path to the
// driver if found; NULL otherwise.
static const char* _Nullable get_boot_mem_driver_path(void)
{
    static const char* gMemDriverTable[] = {
        "/ram",
        "/rom",
        NULL
    };

    int i = 0;
    const char* path;

    while ((path = gMemDriverTable[i++]) != NULL) {
        const errno_t err = DriverCatalog_IsDriverPublished(gDriverCatalog, path);
        
        if (err == EOK) {
            return path;
        }
    }

    return NULL;
}

// Finds a floppy disk to boot from and returns the in-kernel path to it if it
// exists; NULL otherwise.
static const char* _Nullable get_boot_floppy_driver_path(void)
{
    static char* gBootFloppyName = "/fd0";

    for (int i = 0; i < 4; i++) {
        const errno_t err = DriverCatalog_IsDriverPublished(gDriverCatalog, gBootFloppyName);
        
        if (err == EOK) {
            return gBootFloppyName;
        }

        gBootFloppyName[3] = '0' + i;
    }

    return NULL;
}

static void wait_for_disk_load_detected(const char* _Nonnull driverPath)
{
    decl_try_err();
    IOChannelRef chan;
    int tries = 10;

    if ((err = DriverCatalog_OpenDriver(gDriverCatalog, driverPath, kOpen_ReadWrite, &chan)) == EOK) {
        while (tries-- > 0) {
            DiskInfo info;

            if (IOChannel_Ioctl(chan, kDiskCommand_GetInfo, &info) != EOK) {
                break;
            }
            if (info.mediaId != kMediaId_None) {
                break;
            }

            VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(100));
        }
    } 
    IOChannel_Release(chan);
}

// Tries to mount the root filesystem stored on the mass storage device
// represented by 'pDriver'.
static errno_t boot_from_disk(const char* _Nonnull driverPath, bool shouldRetry, FilesystemRef _Nullable * _Nonnull pOutFS)
{
    decl_try_err();
    errno_t lastError = EOK;
    FilesystemRef fs;
    bool shouldPromptForDisk = true;

    // Wait a bit for the disk loaded detection mechanism to actually pick up
    // that a disk is loaded. This may take a couple hundred milliseconds
    // depending on how exactly the driver hardware and software work.
    // We do it this way because we don't want to print a bogus "insert a disk"
    // to the screen although the disk is (mechanically) already loaded, the
    // drive mechanics just hasn't picked this fact up yet.
    wait_for_disk_load_detected(driverPath);


    // Try to boot from the disk
    while (true) {
        fs = NULL;
        err = FilesystemManager_DiscoverAndStartFilesystem(gFilesystemManager, driverPath, NULL, 0, &fs);

        if (err == EOK) {
            break;
        }
        else if (err != ENOMEDIUM && err != lastError) {
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

    print("Booting from %s...\n\n", &driverPath[1]);

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
    const char* memDriverPath = get_boot_mem_driver_path();
    const char* driverPath = NULL;
    bool shouldRetry = false;

    while (true) {
        switch (state) {
            case 0:
                // Boot floppy disk
                driverPath = get_boot_floppy_driver_path();
                shouldRetry = (memDriverPath == NULL) ? true : false;
                break;

            case 1:
                // ROM disk image, if it exists
                driverPath = memDriverPath;
                shouldRetry = false;
                break;

            default:
                state = -1;
                break;
        }
        if (state < 0) {
            break;
        }

        if (driverPath) {
            err = boot_from_disk(driverPath, shouldRetry, &fs);
            if (err == EOK) {
                break;
            }
        }

        state++;
    }

    return fs;
}
