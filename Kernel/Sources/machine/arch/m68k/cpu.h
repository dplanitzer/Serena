//
//  m68k/cpu.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CPU_M68K_H
#define _CPU_M68K_H 1

#include <kern/floattypes.h>
#include <kern/types.h>

// Size of a standard page in bytes
#define CPU_PAGE_SIZE   4096

#define STACK_ALIGNMENT  4


// CPU types
#define CPU_MODEL_68000     0
#define CPU_MODEL_68010     1
#define CPU_MODEL_68020     2
#define CPU_MODEL_68030     3
#define CPU_MODEL_68040     4
#define CPU_MODEL_68060     6

// FPU types
#define FPU_MODEL_NONE      0
#define FPU_MODEL_68881     1
#define FPU_MODEL_68882     2
#define FPU_MODEL_68040     3
#define FPU_MODEL_68060     4


// FPU state frame sizes (fsave/frestore, see M68000PRM p. 6-12)
#define FPU_NULL_STATE_SIZE         4
#define FPU_68040_IDLE_STATE_SIZE   4
#define FPU_68881_IDLE_STATE_SIZE   28
#define FPU_68882_IDLE_STATE_SIZE   60
#define FPU_68040_UNIMP_STATE_SIZE  48
#define FPU_68040_BUSY_STATE_SIZE   96
#define FPU_68881_BUSY_STATE_SIZE   184
#define FPU_68882_BUSY_STATE_SIZE   216
#define FPU_MAX_STATE_SIZE          216


// CPU (68k) address space selector (Alternate function codes)
#define CPU68K_USER_DATA_SPACE  1
#define CPU68K_USER_CODE_SPACE  2
#define CPU68K_SUPER_DATA_SPACE 5
#define CPU68K_SUPER_CODE_SPACE 6
#define CPU68K_CPU_SPACE        7


// Status register
#define CPU_SR_TRACE        0x8000
#define CPU_SR_S            0x2000
#define CPU_SR_IE_MASK      0x700
#define CPU_SR_X            0x10
#define CPU_SR_N            0x08
#define CPU_SR_Z            0x04
#define CPU_SR_V            0x02
#define CPU_SR_C            0x01


// CPU register state (keep in sync with lowmem.i)
typedef struct mcontext {
    
    // Integer state. 68000 or better
    uint32_t    d[8];
    uintptr_t   a[8];
    uintptr_t   usp;
    uintptr_t   pc;
    uint16_t    sr;
    uint16_t    padding;

    // Floating-point state. 68881, 68882, 68040 or better
    uint8_t     fsave[FPU_MAX_STATE_SIZE];  // fsave / frestore data
    float96_t   fp[8];
    uint32_t    fpcr;
    uint32_t    fpsr;
    uint32_t    fpiar;
} mcontext_t;


// Exception stack frame
#pragma pack(1)
typedef struct excpt_frame {
    uint16_t    sr;
    uintptr_t   pc;

    struct {
        uint16_t  format: 4;
        uint16_t  vector: 12;
    }       fv;
    
    union {
        struct Format2 {
            uintptr_t   addr;
        }   f2;
        struct Format3 {
            uintptr_t   ea;
        }   f3;
        struct Format4 {
            uintptr_t   ea;
            uintptr_t   pcFaultedInstr;
        }   f4;
        struct Format7 {
            uintptr_t   ea;
            uint16_t    ssw;
            uint16_t    wb3s;
            uint16_t    wb2s;
            uint16_t    wb1s;
            uint32_t    fa;
            uint32_t    wb3a;
            uint32_t    wb3d;
            uint32_t    wb2a;
            uint32_t    wb2d;
            uint32_t    wb1a;
            uint32_t    wb1d;
            uint32_t    pd1;
            uint32_t    pd2;
            uint32_t    pd3;
        }   f7;
        struct Format9 {
            uintptr_t   ia;
            uint16_t    ir[4];
        }   f9;
        struct FormatA {
            uint16_t    ir0;
            uint16_t    ssw;
            uint16_t    ipsc;
            uint16_t    ipsb;
            uintptr_t   dataCycleFaultAddress;
            uint16_t    ir1;
            uint16_t    ir2;
            uint32_t    dataOutputBuffer;
            uint16_t    ir3;
            uint16_t    ir4;
        }   fa;
        struct FormatB {
            uint16_t    ir0;
            uint16_t    ssw;
            uint16_t    ipsc;
            uint16_t    ipsb;
            uintptr_t   dataCycleFaultAddress;
            uint16_t    ir1;
            uint16_t    ir2;
            uint32_t    dataOutputBuffer;
            uint16_t    ir3;
            uint16_t    ir4;
            uint16_t    ir5;
            uint16_t    ir6;
            uintptr_t   stageBAddress;
            uint16_t    ir7;
            uint16_t    ir8;
            uint32_t    dataInputBuffer;
            uint16_t    ir9;
            uint16_t    ir10;
            uint16_t    ir11;
            uint16_t    version;
            uint16_t    ir[18];
        }   fb;
    }       u;
} excpt_frame_t;
#pragma pack()

#define excpt_frame_isuser(__ef) \
(((__ef)->sr & CPU_SR_S) == 0)

#define excpt_frame_getpc(__ef) \
((__ef)->pc)

#define excpt_frame_setpc(__ef, __pc) \
(__ef)->pc = (uintptr_t)(__pc)


extern unsigned int cpu68k_as_read_byte(void* p, int addr_space);
extern void cpu68k_as_write_byte(void* p, int addr_space, unsigned int val);

#endif /* _CPU_M68K_H */
