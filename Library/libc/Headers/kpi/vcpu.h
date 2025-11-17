//
//  kpi/vcpu.h
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_VCPU_H
#define _KPI_VCPU_H 1

#include <kpi/floattypes.h>
#include <kpi/sched.h>
#include <stdint.h>

#define VCPUID_SELF 0
#define VCPUID_MAIN 1

#define VCPUID_MAIN_GROUP   1


struct vcpu;
typedef struct vcpu* vcpu_t;


// Acquire the VP and immediately resume it
#define VCPU_ACQUIRE_RESUMED    1

typedef void (*vcpu_func_t)(void* _Nullable);

typedef struct _vcpu_acquire_attr {
    vcpu_func_t _Nullable   func;
    void* _Nullable         arg;
    size_t                  stack_size;
    vcpuid_t                groupid;
    sched_params_t     sched_params;
    unsigned int            flags;
    intptr_t                data;
} _vcpu_acquire_attr_t;


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

#endif /* _KPI_VCPU_H */
