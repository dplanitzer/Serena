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

#define VCPUID_SELF 0

struct vcpu;
typedef struct vcpu* vcpu_t;

struct vcpu_key;
typedef struct vcpu_key* vcpu_key_t;


// Acquire the VP and immediately resume it
#define VCPU_ACQUIRE_RESUMED    1

typedef void (*vcpu_func_t)(void* _Nullable);

typedef struct vcpu_attr {
    vcpu_func_t _Nullable   func;
    void* _Nullable         arg;
    size_t                  stack_size;
    vcpuid_t                groupid;
    int                     priority;
    unsigned int            flags;
    vcpu_key_t _Nullable    owner_key;
    const void* _Nullable   owner_value;
} vcpu_attr_t;

#endif /* _KPI_VCPU_H */
