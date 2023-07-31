//
//  Platform.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Platform.h"

CopperScheduler gCopperSchedulerStorage;


// Returns true if the machine is a NTSC machine; false if it is a PAL machine
Bool chipset_is_ntsc(void)
{
    return (chipset_get_version() & (1 << 4)) != 0;
}

// Returns the first memory address that the on-board chipset can not access via DMA.
Byte* _Nonnull chipset_get_mem_limit(void)
{
    switch (chipset_get_version()) {
        case CHIPSET_8370_NTSC:             return (Byte*) (512 * 1024);
        case CHIPSET_8371_PAL:              return (Byte*) (512 * 1024);
        case CHIPSET_8372_rev4_PAL:         return (Byte*) (1 * 1024 * 1024);
        case CHIPSET_8372_rev4_NTSC:        return (Byte*) (1 * 1024 * 1024);
        case CHIPSET_8372_rev5_NTSC:        return (Byte*) (1 * 1024 * 1024);
        case CHIPSET_8374_rev2_PAL:         return (Byte*) (2 * 1024 * 1024);
        case CHIPSET_8374_rev2_NTSC:        return (Byte*) (2 * 1024 * 1024);
        case CHIPSET_8374_rev3_PAL:         return (Byte*) (2 * 1024 * 1024);
        case CHIPSET_8374_rev3_NTSC:        return (Byte*) (2 * 1024 * 1024);
        default:                            return (Byte*) (512 * 1024);
    }
}

// See: <https://eab.abime.net/showthread.php?t=34838>
UInt8 chipset_get_version(void)
{
    volatile UInt16* pVPOSR = (volatile UInt16*)(CUSTOM_BASE + VPOSR);
    UInt16 vposr = *pVPOSR;

    return (vposr >> 8) & 0x7f;
}

UInt8 chipset_get_ramsey_version(void)
{
    volatile UInt8* pRAMSEY = (volatile UInt8*)RAMSEY_CHIP_BASE;
    UInt8 v = *pRAMSEY;

    switch (v) {
        case CHIPSET_RAMSEY_rev04:
        case CHIPSET_RAMSEY_rev07:
            return v;
            
        default:
            return 0;
    }
}
