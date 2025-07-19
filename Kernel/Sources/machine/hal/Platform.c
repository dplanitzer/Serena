//
//  Platform.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <machine/Platform.h>


//
// Chipset
//

// Returns true if the machine is a NTSC machine; false if it is a PAL machine
bool chipset_is_ntsc(void)
{
    return (chipset_get_version() & (1 << 4)) != 0;
}

// See: <https://eab.abime.net/showthread.php?t=34838>
uint8_t chipset_get_version(void)
{
    CHIPSET_BASE_DECL(cp);

    return (*CHIPSET_REG_16(cp, VPOSR) >> 8) & 0x7f;
}

uint8_t chipset_get_ramsey_version(void)
{
    RAMSEY_BASE_DECL(cp);
    const uint8_t v = *RAMSEY_REG_8(cp, RAMSEY_VERSION);

    switch (v) {
        case RAMSEY_rev4:
        case RAMSEY_rev7:
            return v;
            
        default:
            return 0;
    }
}

char* chipset_get_upper_dma_limit(int chipset_version)
{
    char* p;

    switch (chipset_version) {
        case CHIPSET_8370_NTSC:             p = (char*) (512 * 1024); break;
        case CHIPSET_8371_PAL:              p = (char*) (512 * 1024); break;
        case CHIPSET_8372_rev4_PAL:         p = (char*) (1 * 1024 * 1024); break;
        case CHIPSET_8372_rev4_NTSC:        p = (char*) (1 * 1024 * 1024); break;
        case CHIPSET_8372_rev5_NTSC:        p = (char*) (1 * 1024 * 1024); break;
        case CHIPSET_8374_rev2_PAL:         p = (char*) (2 * 1024 * 1024); break;
        case CHIPSET_8374_rev2_NTSC:        p = (char*) (2 * 1024 * 1024); break;
        case CHIPSET_8374_rev3_PAL:         p = (char*) (2 * 1024 * 1024); break;
        case CHIPSET_8374_rev3_NTSC:        p = (char*) (2 * 1024 * 1024); break;
        default:                            p = (char*) (2 * 1024 * 1024); break;
    }

    return p;
}

uint32_t chipset_get_hsync_counter(void)
{
    CIAB_BASE_DECL(cp);
    const uint32_t h = *CIA_REG_8(cp, CIA_TODHI);
    const uint32_t m = *CIA_REG_8(cp, CIA_TODMID);
    const uint32_t l = *CIA_REG_8(cp, CIA_TODLO);

    return (h << 16) | (m << 8) | l; 
}