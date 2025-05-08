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
#include <filesystem/IOChannel.h>
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

static errno_t get_current_disk_id(const char* _Nonnull driverPath, uint32_t* _Nonnull diskId)
{
    decl_try_err();
    IOChannelRef chan;

    if ((err = Catalog_Open(gDriverCatalog, driverPath, kOpen_ReadWrite, &chan)) == EOK) {
        diskinfo_t info;

        err = IOChannel_Ioctl(chan, kDiskCommand_GetInfo, &info);
        if (err == EOK) {
            *diskId = info.diskId;
        }
    } 
    IOChannel_Release(chan);
    return err;
}

static void wait_for_disk_inserted(boot_screen_t* _Nonnull bscr, const char* _Nonnull driverPath, uint32_t* _Nonnull diskId)
{
    decl_try_err();
    IOChannelRef chan;
    bool isWaitingForDisk = false;

    if ((err = Catalog_Open(gDriverCatalog, driverPath, kOpen_ReadWrite, &chan)) == EOK) {
        for (;;) {
            diskinfo_t info;

            err = IOChannel_Ioctl(chan, kDiskCommand_SenseDisk);
            if (err == EOK) {
                IOChannel_Ioctl(chan, kDiskCommand_GetInfo, &info);
                if (info.diskId != *diskId) {
                    *diskId = info.diskId;
                    break;
                }
            }

            if (!isWaitingForDisk) {
                blit_boot_logo(bscr, gFloppyImg_Plane0, gFloppyImg_Width, gFloppyImg_Height);
                isWaitingForDisk = true;
            }

            VirtualProcessor_Sleep(TimeInterval_MakeSeconds(3));
        }
    } 
    IOChannel_Release(chan);

    if (isWaitingForDisk) {
        blit_boot_logo(bscr, gSerenaImg_Plane0, gSerenaImg_Width, gSerenaImg_Height);
    }
}

// SerenaFS is the only FS we support at this time for booting.
static errno_t start_boot_fs(const char* _Nonnull diskPath, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    IOChannelRef chan;
    FilesystemRef fs = NULL;
    ResolvedPath rp;

    try(Catalog_AcquireNodeForPath(gDriverCatalog, diskPath, &rp));
    Inode_Lock(rp.inode);
    err = FilesystemManager_EstablishFilesystem(gFilesystemManager, rp.inode, kOpen_ReadWrite, &fs);
    Inode_Unlock(rp.inode);
    throw_iferr(err);

    try(FilesystemManager_StartFilesystem(gFilesystemManager, fs, ""));
    
    ResolvedPath_Deinit(&rp);
    *pOutFs = fs;
    return EOK;

catch:
    if (fs) {
        FilesystemManager_DisbandFilesystem(gFilesystemManager, fs);
    }
    ResolvedPath_Deinit(&rp);
    *pOutFs = NULL;

    return err;
}

// Tries to mount the root filesystem stored on the mass storage device
// represented by 'pDriver'.
static errno_t boot_from_disk(const char* _Nonnull driverPath, bool shouldRetry, boot_screen_t* _Nonnull bscr, FilesystemRef _Nullable * _Nonnull pOutFS)
{
    decl_try_err();
    uint32_t diskId = 0;
    FilesystemRef fs;

    // Try to boot from the disk
    while (true) {
        wait_for_disk_inserted(bscr, driverPath, &diskId);

        fs = NULL;
        err = start_boot_fs(driverPath, &fs);
        if (err == EOK) {
            break;
        }
        if (!shouldRetry) {
            // No disk or no mountable disk. We have a fallback though so bail
            // out and let the caller try another option.
            *pOutFS = NULL;
            return err;
        }
    }

    printf("Booting from %s...\n\n", &driverPath[1]);

    *pOutFS = fs;
    return EOK;
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
