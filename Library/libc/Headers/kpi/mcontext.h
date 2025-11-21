//
//  kpi/mcontext.h
//  libc
//
//  Created by Dietmar Planitzer on 11/21/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_MCONTEXT_H
#define _KPI_MCONTEXT_H 1

#include <kpi/floattypes.h>
#include <stdint.h>

// Machine context
typedef struct mcontext {
#ifdef __M68K__
    uint32_t    d[8];
    uint32_t    a[8];
    uint32_t    pc;
    uint32_t    sr;     // CCR portion only (bits 0..7); rest is 0

    uint32_t    fpiar;
    uint32_t    fpsr;
    uint32_t    fpcr;
    float96_t   fp[8];
#else
#error "Unknown CPU architecture"
#endif
} mcontext_t;

#endif /* _KPI_MCONTEXT_H */
