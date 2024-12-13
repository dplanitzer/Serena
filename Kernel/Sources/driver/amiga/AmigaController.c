//
//  AmigaController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "AmigaController.h"
#include "DriverCatalog.h"
#include <console/Console.h>
#include <dispatcher/Lock.h>
#include <driver/amiga/floppy/FloppyController.h>
#include <driver/amiga/graphics/GraphicsDriver.h>
#include <driver/amiga/RealtimeClock.h>
#include <driver/amiga/zorro/ZorroController.h>
#include <driver/disk/RamDisk.h>
#include <driver/disk/RomDisk.h>
#include <driver/hid/EventDriver.h>
#include <driver/NullDriver.h>
#include <filesystem/DiskFSContainer.h>
#include <filesystem/IOChannel.h>
#include <filesystem/SerenaDiskImage.h>
#include <System/ByteOrder.h>

extern char _text, _etext, _data, _edata;


final_class_ivars(AmigaController, PlatformController,
);


errno_t AmigaController_Create(PlatformControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(AmigaController, kDriverModel_Sync, 0, (DriverRef*)pOutSelf);
}

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
static errno_t AmigaController_AutoDetectBootMemoryDisk(struct AmigaController* _Nonnull self)
{
    decl_try_err();
    const SMG_Header* _Nonnull smg_hdr = find_rom_rootfs();
    IOChannelRef chan = NULL;

    if (smg_hdr == NULL) {
        return EOK;
    }


    // Create a RAM disk and copy the ROM disk image into it. We assume for now
    // that the disk image is exactly 64k in size.
    DiskDriverRef disk = NULL;
    const char* dmg = ((const char*)smg_hdr) + smg_hdr->headerSize;

    if ((smg_hdr->options & SMG_OPTION_READONLY) == SMG_OPTION_READONLY) {
        try(RomDisk_Create("rom", dmg, smg_hdr->blockSize, smg_hdr->physicalBlockCount, false, (RomDiskRef*)&disk));
        try(Driver_Start((DriverRef)disk));
    }
    else {
        FSContainerRef fsContainer = NULL;

        try(RamDisk_Create("ram", smg_hdr->blockSize, smg_hdr->physicalBlockCount, 128, (RamDiskRef*)&disk));
        try(Driver_Start((DriverRef)disk));

        try(Driver_Open((DriverRef)disk, kOpen_ReadWrite, &chan));
        try(DiskFSContainer_Create(chan, &fsContainer));
        for (LogicalBlockAddress lba = 0; lba < smg_hdr->physicalBlockCount; lba++) {
            DiskBlockRef pb;

            try(FSContainer_AcquireBlock(fsContainer, lba, kAcquireBlock_Replace, &pb));
            memcpy(DiskBlock_GetMutableData(pb), &dmg[lba * smg_hdr->blockSize], DiskBlock_GetByteSize(pb));
            try(FSContainer_RelinquishBlockWriting(fsContainer, pb, kWriteBlock_Sync));
        }
        Object_Release(fsContainer);
        IOChannel_Release(chan);
    }

    Driver_AdoptChild((DriverRef)self, (DriverRef)disk);
    return EOK;

catch:
    if (disk) {
        IOChannel_Release(chan);
        Driver_Terminate((DriverRef)disk);
        Object_Release(disk);
    }
    return err;
}

errno_t AmigaController_start(struct AmigaController* _Nonnull self)
{
    decl_try_err();


    // Graphics Driver
    const ScreenConfiguration* pVideoConfig;
    if (chipset_is_ntsc()) {
        pVideoConfig = &kScreenConfig_NTSC_640_200_60;
        //pVideoConfig = &kScreenConfig_NTSC_640_400_30;
    } else {
        pVideoConfig = &kScreenConfig_PAL_640_256_50;
        //pVideoConfig = &kScreenConfig_PAL_640_512_25;
    }
    
    GraphicsDriverRef graphDriver = NULL;
    try(GraphicsDriver_Create(pVideoConfig, kPixelFormat_RGB_Indexed3, &graphDriver));
    try(Driver_Start((DriverRef)graphDriver));
    Driver_AdoptChild((DriverRef)self, (DriverRef)graphDriver);


    // Event Driver
    EventDriverRef eventDriver = NULL;
    try(EventDriver_Create(graphDriver, &eventDriver));
    try(Driver_Start((DriverRef)eventDriver));
    Driver_AdoptChild((DriverRef)self, (DriverRef)eventDriver);


    // Initialize the console
    ConsoleRef console = NULL;
    try(Console_Create(eventDriver, graphDriver, &console));
    try(Driver_Start((DriverRef)console));
    Driver_AdoptChild((DriverRef)self, (DriverRef)console);


    // Let the kernel know that the console is now available
    PlatformController_NoteConsoleAvailable((PlatformControllerRef)self, console);


    // Null driver
    DriverRef nd = NULL;
    try(NullDriver_Create(&nd));
    try(Driver_Start(nd));
    Driver_AdoptChild((DriverRef)self, nd);


    // Floppy Bus
    FloppyControllerRef fdc = NULL;
    try(FloppyController_Create(&fdc));
    try(Driver_Start((DriverRef)fdc));
    Driver_AdoptChild((DriverRef)self, (DriverRef)fdc);


    // Realtime Clock
    #if 0
    // XXX not yet
    RealtimeClockRef rtcDriver = NULL;
    try(RealtimeClock_Create(gSystemDescription, &rtcDriver));
    try(Driver_Start((DriverRef)rtcDriver));
    Driver_AdoptChild((DriverRef)self, (DriverRef)rtcDriver);
    #endif


    // Zorro Bus
    ZorroControllerRef zorroController = NULL;
    try(ZorroController_Create(&zorroController));
    try(Driver_Start((DriverRef)zorroController));
    Driver_AdoptChild((DriverRef)self, (DriverRef)zorroController);


    // Create a boot ram/rom disk if a disk image exists in ROM
    try(AmigaController_AutoDetectBootMemoryDisk(self));

catch:
    return err;
}

class_func_defs(AmigaController, PlatformController,
override_func_def(start, AmigaController, Driver)
);
