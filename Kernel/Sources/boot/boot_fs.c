//
//  boot_fs.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <krt/krt.h>
#include <klib/klib.h>
#include <log/Log.h>
#include <dispatcher/VirtualProcessor.h>
#include <driver/DriverChannel.h>
#include <driver/amiga/graphics/GraphicsDriver.h>
#include <driver/disk/DiskDriver.h>
#include <filemanager/FileHierarchy.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/DiskContainer.h>
#include <filesystem/IOChannel.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <hal/Platform.h>
#include <Catalog.h>
#include "boot_screen.h"


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
        const errno_t err = Catalog_IsPublished(gDriverCatalog, path);
        
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
    static char* gBootFloppyName = "/hw/fd-bus/fd0";

    for (int i = 0; i < 4; i++) {
        const errno_t err = Catalog_IsPublished(gDriverCatalog, gBootFloppyName);
        
        if (err == EOK) {
            return gBootFloppyName;
        }

        gBootFloppyName[6] = '0' + i;
    }

    return NULL;
}

static void wait_for_disk_change(const char* _Nonnull driverPath, int maxTries, MediaId* _Nonnull mediaId)
{
    decl_try_err();
    IOChannelRef chan;
    int tries = maxTries;

    if ((err = Catalog_Open(gDriverCatalog, driverPath, kOpen_ReadWrite, &chan)) == EOK) {
        while (tries-- > 0) {
            DiskInfo info;

            if (IOChannel_Ioctl(chan, kDiskCommand_GetInfo, &info) != EOK) {
                break;
            }
            if (info.mediaId != *mediaId) {
                *mediaId = info.mediaId;
                break;
            }

            VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(100));
        }
    } 
    IOChannel_Release(chan);
}

static void ask_user_for_new_disk(boot_screen_t* _Nonnull bscr, const char* _Nonnull driverPath, MediaId* _Nonnull mediaId)
{
    blit_boot_logo(bscr, gFloppyImg_Plane0, gFloppyImg_Width, gFloppyImg_Height);


    // Wait for the user to insert a different disk
    wait_for_disk_change(driverPath, INT_MAX, mediaId);


    blit_boot_logo(bscr, gSerenaImg_Plane0, gSerenaImg_Width, gSerenaImg_Height);
}

// SerenaFS is the only FS we support at this time for booting.
static errno_t start_boot_fs(const char* _Nonnull driverPath, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    IOChannelRef chan;
    FSContainerRef fsContainer = NULL;
    FilesystemRef fs = NULL;

    try(Catalog_Open(gDriverCatalog, driverPath, kOpen_ReadWrite, &chan));
    try(DiskContainer_Create(chan, &fsContainer));
    try(SerenaFS_Create(fsContainer, (SerenaFSRef*)&fs));
    try(FilesystemManager_StartFilesystem(gFilesystemManager, fs, ""));

catch:
    if (err != EOK) {
        Object_Release(fs);
    }
    Object_Release(fsContainer);
    IOChannel_Release(chan);

    *pOutFs = fs;
    return err;
}

// Tries to mount the root filesystem stored on the mass storage device
// represented by 'pDriver'.
static errno_t boot_from_disk(const char* _Nonnull driverPath, bool shouldRetry, boot_screen_t* _Nonnull bscr, FilesystemRef _Nullable * _Nonnull pOutFS)
{
    decl_try_err();
    MediaId curMediaId = kMediaId_None;
    FilesystemRef fs;

    // Wait a bit for the disk loaded detection mechanism to actually pick up
    // that a disk is loaded. This may take a couple hundred milliseconds
    // depending on how exactly the driver hardware and software work.
    // We do it this way because we don't want to print a bogus "insert a disk"
    // to the screen although the disk is (mechanically) already loaded, the
    // drive mechanics just hasn't picked this fact up yet.
    wait_for_disk_change(driverPath, 10, &curMediaId);


    // Try to boot from the disk
    while (true) {
        fs = NULL;
        
        err = start_boot_fs(driverPath, &fs);

        if (err == EOK) {
            break;
        }
        if (!shouldRetry) {
            // No disk or no mountable disk. We have a fallback though so bail
            // out and let the caller try another option.
            return err;
        }

        ask_user_for_new_disk(bscr, driverPath, &curMediaId);
    }

    printf("Booting from %s...\n\n", &driverPath[1]);

    *pOutFS = fs;
    return EOK;

catch:
    Object_Release(fs);
    *pOutFS = NULL;
    return err;
}

// Locates the boot device and creates the boot filesystem. Halts the machine if
// a boot device/filesystem can not be found.
FilesystemRef _Nullable create_boot_filesystem(boot_screen_t* _Nonnull bscr)
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
            err = boot_from_disk(driverPath, shouldRetry, bscr, &fs);
            if (err == EOK) {
                break;
            }
        }

        state++;
    }

    return fs;
}

// Creates the root file hierarchy based on the detected boot filesystem. Halts
// the machine if anything goes wrong.
FileHierarchyRef _Nonnull create_root_file_hierarchy(boot_screen_t* _Nonnull bscr)
{
    decl_try_err();
    FilesystemRef fs;
    FileHierarchyRef fh;

    fs = create_boot_filesystem(bscr);
    if (fs == NULL) {
        printf("No boot device found.\nHalting...\n");
        while(true);
        /* NOT REACHED */
    }

    try(FileHierarchy_Create(fs, &fh));
    return fh;

catch:
    printf("Unable to boot (%d).\nHalting...\n", err);
    while(true);
    /* NOT REACHED */
}
