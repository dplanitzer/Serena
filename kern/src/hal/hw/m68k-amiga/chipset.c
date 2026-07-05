//
//  chipset.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "chipset.h"


// Blocks the caller until the video beam has reached the bottom of the current
// video frame.
void chipset_wait_bof(void)
{
    CHIPSET_BASE_DECL(cp);
    const uint32_t ll = (chipset_is_ntsc()) ? 253 : 303;

    for (;;) {
        const uint32_t vposr = (*CHIPSET_REG_32(cp, VPOSR) & 0x1ff00) >> 8;

        if (vposr == ll) {
            break;
        }
    }
}

// Returns true if the machine is a NTSC machine; false if it is a PAL machine
bool chipset_is_ntsc(void)
{
    CHIPSET_BASE_DECL(cp);

    return ((*CHIPSET_REG_16(cp, VPOSR) >> 8) & 0x10) != 0;
}

// See: <https://eab.abime.net/showthread.php?t=34838>
uint8_t chipset_get_agnus_version(void)
{
    CHIPSET_BASE_DECL(cp);

    return (*CHIPSET_REG_16(cp, VPOSR) >> 8) & 0x6f;
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

char* chipset_get_upper_dma_limit(int agnus_version)
{
    char* p;

    switch (agnus_version) {
        case AGNUS_8371:      p = (char*) (512 * 1024); break;
        case AGNUS_8372_rev4: p = (char*) (1 * 1024 * 1024); break;
        case AGNUS_8372_rev5: p = (char*) (1 * 1024 * 1024); break;
        case AGNUS_8374_rev2: p = (char*) (2 * 1024 * 1024); break;
        case AGNUS_8374_rev3: p = (char*) (2 * 1024 * 1024); break;
        default:              p = (char*) (2 * 1024 * 1024); break;
    }

    return p;
}
