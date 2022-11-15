//
//  chipset.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Platform.h"


// Returns true if the machine is a NTSC machine; false if it is a PAL machine
Bool chipset_is_ntsc(void)
{
    return (chipset_get_version() & (1 << 4)) != 0;
}

// Returns the first address that the on-board chipset can not access via DMA.
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
        default:                            return (Byte*) -1;
    }
}
