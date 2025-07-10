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

// Acquire the VP and immediately resume it
#define VCPU_ACQUIRE_RESUMED    1

typedef struct vcpu_acquire_params {
    void                    (* _Nonnull func)(void);
    void* _Nullable         context;
    size_t                  user_stack_size;
    vcpuid_t                vpgid;
    int                     priority;
    unsigned int            flags;
} vcpu_acquire_params_t;

#endif /* _KPI_VCPU_H */
