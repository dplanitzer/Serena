//
//  Platform.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Platform.h"

//
// CPU
//

// Returns the model name of the CPU
const char* _Nonnull cpu_get_model_name(int8_t cpu_model)
{
    switch (cpu_model) {
        case CPU_MODEL_68000:
            return "68000";
            
        case CPU_MODEL_68010:
            return "68010";
            
        case CPU_MODEL_68020:
            return "68020";
            
        case CPU_MODEL_68030:
            return "68030";
            
        case CPU_MODEL_68040:
            return "68040";
            
        case CPU_MODEL_68060:
            return "68060";
            
        default:
            return "??";
    }
}


//
// FPU
//

// Returns the model name of the FPU
const char* _Nonnull fpu_get_model_name(int8_t fpu_model)
{
    switch (fpu_model) {
        case FPU_MODEL_NONE:
            return "none";
            
        case FPU_MODEL_68881:
            return "68881";
            
        case FPU_MODEL_68882:
            return "68882";
            
        case FPU_MODEL_68040:
            return "68040";
            
        case FPU_MODEL_68060:
            return "68060";
            
        default:
            return "??";
    }
}


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
    volatile uint8_t* pRAMSEY = (volatile uint8_t*)RAMSEY_CHIP_BASE;
    uint8_t v = *pRAMSEY;

    switch (v) {
        case CHIPSET_RAMSEY_rev04:
        case CHIPSET_RAMSEY_rev07:
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