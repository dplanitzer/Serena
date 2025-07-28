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


// Quality of Service level. From highest to lowest.
// VCPU_QOS_REALTIME: kernel will minimize the scheduling latency. Realtime is always scheduled before anything else
// VCPU_QOS_IDLE: no guarantee with regards to schedule latency. Only scheduled if there is nothing to schedule for a DISPATCH_QOS_XXX > VCPU_QOS_IDLE
#define VCPU_QOS_REALTIME       4
#define VCPU_QOS_INTERACTIVE    3
#define VCPU_QOS_UTILITY        2
#define VCPU_QOS_BACKGROUND     1
#define VCPU_QOS_IDLE           0

#define VCPU_QOS_COUNT          5


// Priorities per QoS level
#define VCPU_PRI_HIGHEST   5
#define VCPU_PRI_NORMAL    0
#define VCPU_PRI_LOWEST   -6

#define VCPU_PRI_COUNT     12



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
