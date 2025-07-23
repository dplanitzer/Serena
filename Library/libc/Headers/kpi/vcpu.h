//
//  kpi/vcpu.h
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_VCPU_H
#define _KPI_VCPU_H 1

#include <kpi/types.h>
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
    int                     priority;
    unsigned int            flags;
    intptr_t                data;
} _vcpu_acquire_attr_t;

#endif /* _KPI_VCPU_H */
