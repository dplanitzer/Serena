//
//  arch/m68k/cpu.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _ARCH_M68K_CPU_H
#define _ARCH_M68K_CPU_H 1

#include <arch/floattypes.h>

// Size of a standard page in bytes
#define CPU_PAGE_SIZE   4096


// Machine context (XXX will go away)
typedef struct mcontext {
    unsigned long   d[8];
    unsigned long   a[8];
    unsigned long   pc;
    unsigned long   sr;     // CCR portion only (bits 0..7); rest is 0

    unsigned long   fpiar;
    unsigned long   fpsr;
    unsigned long   fpcr;
    float96_t       fp[8];
} mcontext_t;


typedef struct vcpu_state_68k {
    unsigned long   d[8];
    unsigned long   a[8];
    unsigned long   pc;
    unsigned long   sr;     // CCR portion only (bits 0..7); rest is 0
} vcpu_state_68k_t;

typedef struct vcpu_state_68k_float {
    unsigned long   fpiar;
    unsigned long   fpsr;
    unsigned long   fpcr;
    float96_t       fp[8];
} vcpu_state_68k_float_t;

#define VCPU_STATE_68K          1
#define VCPU_STATE_68K_FLOAT    2

#endif /* _ARCH_M68K_CPU_H */
