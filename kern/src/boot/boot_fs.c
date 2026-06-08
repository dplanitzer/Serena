//
//  boot_fs.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <driver/IOCatalog.h>
#include <driver/disk/DiskDriver.h>
#include <filemanager/FileHierarchy.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/DiskContainer.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <handler/DriverHandler.h>
#include <kei/kei.h>
#include <kern/log.h>
#include <kpi/file.h>
#include <sched/delay.h>
#include <sched/vcpu.h>
#include "boot_screen.h"

extern void auto_discover_boot_rd(void);


// Finds a RAM or ROM disk to boot from and returns the in-kernel path to the
// driver if found; NULL otherwise.
static const char* _Nullable get_boot_mem_driver_path(void)
{
    static const char* gMemDriverTable[] = {
        "/vd-bus/rd0",
        NULL
    };

    int i = 0;
    const char* path;

    while ((path = gMemDriverTable[i++]) != NULL) {
        const errno_t err = IOCatalog_IsPublished(gIOCatalog, path);
        
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
        const errno_t err = IOCatalog_IsPublished(gIOCatalog, gBootFloppyName);
        
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
    HandlerRef hnd;

    if ((err = IOCatalog_Open(gIOCatalog, driverPath, O_RDWR, &hnd)) == EOK) {
        disk_info_t info;

        err = DriverHandler_Ioctl(hnd, kDiskCommand_GetDiskInfo, &info);
        if (err == EOK) {
            *diskId = info.diskId;
        }

        Handler_Close(hnd);
        Object_Release(hnd);
    }
    return err;
}

static void wait_for_disk_inserted(bt_screen_t* _Nonnull bscr, const char* _Nonnull driverPath, uint32_t* _Nonnull diskId)
{
    decl_try_err();
    HandlerRef hnd;
    bool isWaitingForDisk = false;

    if ((err = IOCatalog_Open(gIOCatalog, driverPath, O_RDWR, &hnd)) == EOK) {
        for (;;) {
            disk_info_t info;

            err = DriverHandler_Ioctl(hnd, kDiskCommand_SenseDisk);
            if (err == EOK) {
                DriverHandler_Ioctl(hnd, kDiskCommand_GetDiskInfo, &info);
                if (info.diskId != *diskId) {
                    *diskId = info.diskId;
                    break;
                }
            }

            if (!isWaitingForDisk) {
                bt_drawicon(bscr, &g_icon_floppy);
                isWaitingForDisk = true;
            }

            delay_sec(3);
        }

        Handler_Close(hnd);
        Object_Release(hnd);
    } 

    if (isWaitingForDisk) {
        bt_drawicon(bscr, &g_icon_serena);
    }
}

static errno_t create_boot_fs(InodeRef _Nonnull driverNode, unsigned int mode, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    FSContainerRef fsContainer = NULL;
    FilesystemRef fs = NULL;

    try(DiskContainer_Create(driverNode, mode, &fsContainer));
    try(SerenaFS_Create(fsContainer, (SerenaFSRef*)&fs));

catch:
    Object_Release(fsContainer);
    *pOutFs = fs;
    return err;
}

// SerenaFS is the only FS we support at this time for booting.
static errno_t start_boot_fs(const char* _Nonnull diskPath, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    HandlerRef chan;
    FilesystemRef fs = NULL;
    ResolvedPath rp;

    try(IOCatalog_AcquireNodeForPath(gIOCatalog, diskPath, &rp));
    Inode_Lock(rp.inode);
    err = create_boot_fs(rp.inode, O_RDWR, &fs);
    Inode_Unlock(rp.inode);
    throw_iferr(err);

    try(FilesystemManager_RegisterFilesystem(gFilesystemManager, fs));
    try(Filesystem_Start(fs, ""));
    
    ResolvedPath_Deinit(&rp);
    *pOutFs = fs;
    return EOK;

catch:
    if (fs) {
        FilesystemManager_DeregisterFilesystem(gFilesystemManager, fs);
    }
    ResolvedPath_Deinit(&rp);
    *pOutFs = NULL;

    return err;
}

// Tries to mount the root filesystem stored on the mass storage device
// represented by 'pDriver'.
static errno_t boot_from_disk(const char* _Nonnull driverPath, bool shouldRetry, bt_screen_t* _Nonnull bscr, FilesystemRef _Nullable * _Nonnull pOutFS)
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
FilesystemRef _Nullable create_boot_filesystem(bt_screen_t* _Nonnull bscr)
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
FileHierarchyRef _Nonnull create_root_file_hierarchy(bt_screen_t* _Nonnull bscr)
{
    decl_try_err();
    FilesystemRef fs;
    FileHierarchyRef fh;

    auto_discover_boot_rd();

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
