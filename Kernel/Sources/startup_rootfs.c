//
//  startup.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <krt/krt.h>
#include <klib/klib.h>
#include <dispatcher/VirtualProcessorPool.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverCatalog.h>
#include <driver/amiga/floppy/FloppyDriver.h>
#include <driver/disk/RamDisk.h>
#include <driver/disk/RomDisk.h>
#include <filesystem/DiskFSContainer.h>
#include <filesystem/FilesystemManager.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <filesystem/SerenaDiskImage.h>
#include <hal/MonotonicClock.h>
#include <hal/Platform.h>
#include <System/ByteOrder.h>
#include "BootAllocator.h"

extern char _text, _etext, _data, _edata;


// Scans the ROM area following the end of the kernel looking for an embedded
// Serena disk image with a root filesystem.
static const SMG_Header* _Nullable find_rom_rootfs(void)
{
    const size_t txt_size = &_etext - &_text;
    const size_t dat_size = &_edata - &_data;
    const char* ps = (const char*)(BOOT_ROM_BASE + txt_size + dat_size);
    const uint32_t* pe4 = (const uint32_t*)__min((const char*)(BOOT_ROM_BASE + BOOT_ROM_SIZE), ps + CPU_PAGE_SIZE);
    const uint32_t* p4 = (const uint32_t*)__Ceil_Ptr_PowerOf2(ps, 4);
    const SMG_Header* smg_hdr = NULL;
    const uint32_t smg_sig = UInt32_HostToBig(SMG_SIGNATURE);

    while (p4 < pe4) {
        if (*p4 == smg_sig) {
            smg_hdr = (const SMG_Header*)p4;
            break;
        }

        p4++;
    }

    return smg_hdr;
}

// Scans the ROM area following the end of the kernel for a embedded Serena disk
// image with the root filesystem.
static bool try_rootfs_from_rom(const SMG_Header* _Nonnull smg_hdr)
{
    decl_try_err();
    const char* romDiskName = "rom";
    const char* ramDiskName = "ram0";
    const char* diskName = NULL;
    const char* dmg = ((const char*)smg_hdr) + smg_hdr->headerSize;
    DiskDriverRef disk;
    FSContainerRef fsContainer;
    FilesystemRef fs;

    // Create a RAM disk and copy the ROM disk image into it. We assume for now
    // that the disk image is exactly 64k in size.
    print("Booting from ROM...\n\n");
    if ((smg_hdr->options & SMG_OPTION_READONLY) == SMG_OPTION_READONLY) {
        try(RomDisk_Create(romDiskName, dmg, smg_hdr->blockSize, smg_hdr->physicalBlockCount, false, (RomDiskRef*)&disk));
        try(Driver_Start((DriverRef)disk));
        diskName = romDiskName;
    }
    else {
        try(RamDisk_Create(ramDiskName, smg_hdr->blockSize, smg_hdr->physicalBlockCount, 128, (RamDiskRef*)&disk));
        try(Driver_Start((DriverRef)disk));
        diskName = ramDiskName;

        for (LogicalBlockAddress lba = 0; lba < smg_hdr->physicalBlockCount; lba++) {
            try(DiskDriver_PutBlock(disk, &dmg[lba * smg_hdr->blockSize], lba));
        }
    }


    // Create a SerenaFS instance and mount it as the root filesystem on the RAM
    // disk
    const DriverId diskId = DriverCatalog_GetDriverIdForName(gDriverCatalog, diskName);
    try(DiskFSContainer_Create(diskId, &fsContainer));
    try(SerenaFS_Create(fsContainer, (SerenaFSRef*)&fs));
    try(FilesystemManager_Mount(gFilesystemManager, fs, NULL, 0, NULL));

    return true;

catch:
    print("Error: unable to mount ROM root FS: %d\n", err);
    return false;
}

// Tries to mount the root filesystem from a floppy disk in drive 0.
static bool try_rootfs_from_fd0(bool hasFallback)
{
    decl_try_err();
    FSContainerRef fsContainer;
    FilesystemRef fs;
    bool shouldPromptForDisk = true;
    const DriverId diskId = DriverCatalog_GetDriverIdForName(gDriverCatalog, kFloppyDrive0Name);

    if (diskId == kDriverId_None) {
        throw(ENODEV);
    }

    try(DiskFSContainer_Create(diskId, &fsContainer));
    try(SerenaFS_Create(fsContainer, (SerenaFSRef*)&fs));

    while (true) {
        err = FilesystemManager_Mount(gFilesystemManager, fs, NULL, 0, NULL);

        if (err == EOK) {
            break;
        }
        else if (err == EDISKCHANGE) {
            // This means that the user inserted a new disk and that the disk
            // hardware isn't able to automatically pick this change up on its
            // own. Just try mounting again. 2nd time around should work.
            continue;
        }
        else if (err != ENOMEDIUM) {
            print("Error: %d\n\n", err);
            shouldPromptForDisk = true;
        }

        if (hasFallback) {
            // No disk or no mountable disk. We have a fallback though so bail
            // out and let the caller try the fallback option.
            return false;
        }

        if (shouldPromptForDisk) {
            print("Please insert a Serena boot disk...\n\n");
            shouldPromptForDisk = false;
        }

        VirtualProcessor_Sleep(TimeInterval_MakeSeconds(1));
    }
    print("Booting from fd0...\n\n");

    return true;

catch:
    print("Error: unable to load root fs from fd0: %d\n", err);
    return false;
}

// Locates the root filesystem and mounts it.
void init_root_filesystem(void)
{
    const SMG_Header* rom_dmg = find_rom_rootfs();

    if (try_rootfs_from_fd0(rom_dmg != NULL)) {
        return;
    }

    if (try_rootfs_from_rom(rom_dmg)) {
        return;
    }


    print("Halting\n");
    while(true);
    /* NOT REACHED */
}
