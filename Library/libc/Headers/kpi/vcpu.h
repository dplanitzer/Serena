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


// Acquire the VP and immediately resume it
#define VCPU_ACQUIRE_RESUMED    1

typedef void (*vcpu_start_t)(void* _Nullable);

typedef struct vcpu_acquire_params {
    vcpu_start_t _Nullable  func;
    void* _Nullable         arg;
    size_t                  stack_size;
    vcpuid_t                groupid;
    int                     priority;
    unsigned int            flags;
} vcpu_acquire_params_t;

#endif /* _KPI_VCPU_H */
