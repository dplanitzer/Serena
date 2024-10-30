//
//  startup.c
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
#include <driver/disk/RamDisk.h>
#include <driver/disk/RomDisk.h>
#include <filesystem/DiskFSContainer.h>
#include <filesystem/FilesystemManager.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <filesystem/SerenaDiskImage.h>
#include <System/ByteOrder.h>

#define MAX_NAME_LENGTH 16

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
static DriverId get_boot_mem_disk_id(const SMG_Header* _Nonnull smg_hdr)
{
    decl_try_err();
    const char* romDiskName = "rom";
    const char* ramDiskName = "ram0";
    const char* diskName = NULL;
    const char* dmg = ((const char*)smg_hdr) + smg_hdr->headerSize;
    DiskDriverRef disk;

    // Create a RAM disk and copy the ROM disk image into it. We assume for now
    // that the disk image is exactly 64k in size.
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
    return DriverCatalog_GetDriverIdForName(gDriverCatalog, diskName);

catch:
    return kDriverId_None;
}

// Tries to mount the root filesystem from a floppy disk in drive 0.
static DriverId get_boot_floppy_disk_id(void)
{
    return DriverCatalog_GetDriverIdForName(gDriverCatalog, kFloppyDrive0Name);
}


// Tries to mount the root filesystem from a floppy disk in drive 0.
static errno_t boot_from_disk(DriverId diskId, bool shouldRetry)
{
    decl_try_err();
    errno_t lastError = EOK;
    FSContainerRef fsContainer;
    FilesystemRef fs;
    bool shouldPromptForDisk = true;

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

    return EOK;

catch:
    return err;
}

// Locates the root filesystem and mounts it.
void init_root_filesystem(void)
{
    decl_try_err();
    const SMG_Header* rom_dmg = find_rom_rootfs();
    int state = 0;
    DriverId diskId = kDriverId_None;
    bool shouldRetry = false;

    while (true) {
        switch (state) {
            case 0:
                // Boot floppy disk
                diskId = get_boot_floppy_disk_id();
                shouldRetry = (rom_dmg == NULL);
                break;

            case 1:
                // ROM disk image, if it exists
                diskId = (rom_dmg) ? get_boot_mem_disk_id(rom_dmg) : kDriverId_None;
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
            err = boot_from_disk(diskId, shouldRetry);
            if (err == EOK) {
                return;
            }
        }

        state++;
    }


    // No luck, give up
    print("No boot device found.\nHalting...\n");
    while(true);
    /* NOT REACHED */
}
