//
//  Platform.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Platform.h"

//
// CPU
//

// Returns the model name of the CPU
const Character* _Nonnull cpu_get_model_name(Int8 cpu_model)
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
const Character* _Nonnull fpu_get_model_name(Int8 fpu_model)
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
Bool chipset_is_ntsc(void)
{
    return (chipset_get_version() & (1 << 4)) != 0;
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

Byte* chipset_get_upper_dma_limit(Int chipset_version)
{
    Byte* p;

    switch (chipset_version) {
        case CHIPSET_8370_NTSC:             p = (Byte*) (512 * 1024); break;
        case CHIPSET_8371_PAL:              p = (Byte*) (512 * 1024); break;
        case CHIPSET_8372_rev4_PAL:         p = (Byte*) (1 * 1024 * 1024); break;
        case CHIPSET_8372_rev4_NTSC:        p = (Byte*) (1 * 1024 * 1024); break;
        case CHIPSET_8372_rev5_NTSC:        p = (Byte*) (1 * 1024 * 1024); break;
        case CHIPSET_8374_rev2_PAL:         p = (Byte*) (2 * 1024 * 1024); break;
        case CHIPSET_8374_rev2_NTSC:        p = (Byte*) (2 * 1024 * 1024); break;
        case CHIPSET_8374_rev3_PAL:         p = (Byte*) (2 * 1024 * 1024); break;
        case CHIPSET_8374_rev3_NTSC:        p = (Byte*) (2 * 1024 * 1024); break;
        default:                            p = (Byte*) (2 * 1024 * 1024); break;
    }

    return p;
}