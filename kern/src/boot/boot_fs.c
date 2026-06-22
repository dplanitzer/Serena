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
#include <kei/kei.h>
#include <kern/kalloc.h>
#include <kern/log.h>
#include <kpi/fd.h>
#include <kpi/file.h>
#include <sched/delay.h>
#include <sched/vcpu.h>
#include <limits.h>
#include "boot_screen.h"

IOCATS_DEF(g_ram_cats, IODISK_RAMDISK);
IOCATS_DEF(g_rom_cats, IODISK_ROMDISK);
IOCATS_DEF(g_fd_cats,  IODISK_FLOPPY);

extern void auto_discover_boot_rd(void);


// Finds a RAM or ROM disk to boot from and returns a strong reference to the
// driver if found; NULL otherwise.
static DiskDriverRef _Nullable get_boot_mem_driver(void)
{
    DiskDriverRef drv = NULL;

    drv = (DiskDriverRef) IOCatalog_CopyBestMatchingDriver(gIOCatalog, g_ram_cats);
    if (drv == NULL) {
        drv = (DiskDriverRef) IOCatalog_CopyBestMatchingDriver(gIOCatalog, g_rom_cats);
    }

    return drv;
}

static int comp_by_boot_pri(DiskDriverRef* _Nonnull p0, DiskDriverRef* _Nonnull p1)
{
    const int pri0 = DiskDriver_GetBootPriority(p0[0]);
    const int pri1 = DiskDriver_GetBootPriority(p1[0]);

    if (pri0 < pri1) return 1;
    if (pri0 > pri1) return -1;
    return 0;
}

// Finds a floppy disk to boot from and returns a strong reference to it if it
// exists; NULL otherwise.
static DiskDriverRef _Nullable get_boot_floppy_driver(void)
{
    decl_try_err();
    DriverRef* drivers = NULL;
    DiskDriverRef drv = NULL;
    size_t count = 0;
    static int g_pri_cutoff = INT_MAX;
    
    err = IOCatalog_CopyMatchingDrivers(gIOCatalog, g_fd_cats, &drivers);
    if (err != EOK) {
        return NULL;
    }


    // Figure out how many drivers matched
    while (drivers[count]) {
        count++;
    }


    // Sort by descending boot priority
    qsort(drivers, count, sizeof(void*), (int (*)(const void*, const void*))comp_by_boot_pri);

    
    // Pick the disk with the priority equal or just below the current priority
    // cutoff
    for (size_t i = 0; (i < count) && (g_pri_cutoff >= 0); i++) {
        const int curPri = DiskDriver_GetBootPriority(drivers[i]);

        if (curPri <= g_pri_cutoff) {
            g_pri_cutoff = curPri - 1;
            drv = Object_Retain(drivers[i]);
            break;
        }
    }

    
    // Release all drivers
    for (size_t i = 0; i < count; i++) {
        Object_Release(drivers[i]);
    }
    kfree(drivers);

    return drv;
}

static void wait_for_disk_inserted(bt_screen_t* _Nonnull bscr, DiskDriverRef _Nonnull disk, uint32_t* _Nonnull diskId)
{
    decl_try_err();
    bool isWaitingForDisk = false;

    err = Driver_Open(disk, O_RDWR, NULL);
    if (err == EOK) {
        for (;;) {
            disk_info_t info;

            err = DiskDriver_SenseDisk(disk);
            if (err == EOK) {
                DiskDriver_GetDiskInfo(disk, &info);
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

        Driver_Close(disk);
    } 

    if (isWaitingForDisk) {
        bt_drawicon(bscr, &g_icon_serena);
    }
}

// SerenaFS is the only FS we support at this time for booting.
static errno_t start_boot_fs(DiskDriverRef _Nonnull disk, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    FSContainerRef fsContainer = NULL;
    FilesystemRef fs = NULL;

    try(DiskContainer_Create(disk, O_RDWR, &fsContainer));
    try(SerenaFS_Create(fsContainer, (SerenaFSRef*)&fs));

    try(FilesystemManager_RegisterFilesystem(gFilesystemManager, fs));
    try(Filesystem_Start(fs, ""));
   
    Object_Release(fsContainer);
    *pOutFs = fs;
    return EOK;

catch:
    Object_Release(fsContainer);

    if (fs) {
        FilesystemManager_DeregisterFilesystem(gFilesystemManager, fs);
    }

    *pOutFs = NULL;
    return err;
}

// Tries to mount the root filesystem stored on the mass storage device
// represented by 'disk'.
static errno_t boot_from_disk(DiskDriverRef _Nonnull disk, bool shouldRetry, bt_screen_t* _Nonnull bscr, FilesystemRef _Nullable * _Nonnull pOutFS)
{
    decl_try_err();
    uint32_t diskId = 0;
    FilesystemRef fs;

    // Try to boot from the disk
    while (true) {
        wait_for_disk_inserted(bscr, disk, &diskId);

        fs = NULL;
        err = start_boot_fs(disk, &fs);
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

//    printf("Booting from %s...\n\n", &driverPath[1]);

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
    DiskDriverRef memDisk = NULL; //get_boot_mem_driver();
    DiskDriverRef disk = NULL;
    bool shouldRetry = false;

    while (true) {
        switch (state) {
            case 0:
                // Boot floppy disk
                disk = get_boot_floppy_driver();
                shouldRetry = (memDisk == NULL) ? true : false;
                break;

            case 1:
                // ROM disk image, if it exists
                disk = memDisk;
                shouldRetry = false;
                break;

            default:
                state = -1;
                break;
        }
        if (state < 0) {
            break;
        }

        if (disk) {
            err = boot_from_disk(disk, shouldRetry, bscr, &fs);

            Object_Release(disk);
            disk = NULL;
            
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
