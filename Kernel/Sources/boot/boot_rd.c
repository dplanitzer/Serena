//
//  boot_rd.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <driver/DriverChannel.h>
#include <driver/DriverManager.h>
#include <driver/PlatformController.h>
#include <driver/disk/RamDisk.h>
#include <driver/disk/RomDisk.h>
#include <driver/pseudo/VDMDriver.h>
#include <filesystem/DiskContainer.h>
#include <filesystem/IOChannel.h>
#include <filesystem/SerenaDiskImage.h>
#include <kern/string.h>
#include <kpi/fcntl.h>


// Checks whether the platform controller is able to provide a bootable disk image
// for a ROM/RAM disk and creates a ROM/RAM disk with the name '/vd-bus/rd0' from
// that image if so. Otherwise does nothing.
void auto_discover_boot_rd(void)
{
    decl_try_err();
    const SMG_Header* _Nonnull smg_hdr = PlatformController_GetBootImage(gPlatformController);

    if (smg_hdr == NULL) {
        return;
    }


    // Open the VDM
    IOChannelRef vdmChannel = NULL;
    try(DriverManager_Open(gDriverManager, "/vd-bus/self", O_RDWR, &vdmChannel));
    VDMDriverRef vdm = DriverChannel_GetDriverAs(vdmChannel, VDMDriver);


    // Create a RAM disk and copy the ROM disk image into it. We assume for now
    // that the disk image is exactly 64k in size.
    IOChannelRef chan = NULL;
    FSContainerRef fsContainer = NULL;
    const char* dmg = ((const char*)smg_hdr) + smg_hdr->headerSize;

    if ((smg_hdr->options & SMG_OPTION_READONLY) == SMG_OPTION_READONLY) {
        try(VDMDriver_CreateRomDisk(vdm, "rd0", smg_hdr->blockSize, smg_hdr->physicalBlockCount, dmg));
    }
    else {
        try(VDMDriver_CreateRamDisk(vdm, "rd0", smg_hdr->blockSize, smg_hdr->physicalBlockCount, 128));

        try(DriverManager_Open(gDriverManager, "/vd-bus/rd0", O_RDWR, &chan));
        try(DiskContainer_Create(chan, &fsContainer));

        for (blkno_t lba = 0; lba < smg_hdr->physicalBlockCount; lba++) {
            FSBlock blk;

            try(FSContainer_MapBlock(fsContainer, lba, kMapBlock_Replace, &blk));
            memcpy(blk.data, &dmg[lba * smg_hdr->blockSize], FSContainer_GetBlockSize(fsContainer));
            try(FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_Sync));
        }
    }

catch:
    Object_Release(fsContainer);
    IOChannel_Release(chan);
    IOChannel_Release(vdmChannel);
}
