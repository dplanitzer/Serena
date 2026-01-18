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


// Machine context
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

#endif /* _ARCH_M68K_CPU_H */
