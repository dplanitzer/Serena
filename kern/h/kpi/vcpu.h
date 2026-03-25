//
//  kpi/vcpu.h
//  kpi
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_VCPU_H
#define _KPI_VCPU_H 1

#include <arch/floattypes.h>
#include <kpi/types.h>
#include <stdint.h>

#define VCPUID_SELF 0
#define VCPUID_MAIN 1

#define VCPUID_MAIN_GROUP   1


// Quality of Service level. From highest to lowest.
// Note that the highest priority of Realtime and the lowest priority of
// Background are privileged priority bands that owned by the scheduler.
// 
// SCHED_QOS_REALTIME:
//  - fixed priority
//  - smallest quantum
//  - minimized latency on wakeup
//  -- eg used for animations, video  & audio playback
//
// SCHED_QOS_URGENT:
//  - dynamic priority
//  - longer quantum compared to realtime QoS
//  - short latency on wakeup
//  -- eg used for I/O drivers
//
// SCHED_QOS_INTERACTIVE:
//  - dynamic priority
//  - longer quantum compared to urgent QoS
//  - short latency on wakeup
//  -- eg used for apps the user is interacting with
//
// SCHED_QOS_UTILITY:
//  - dynamic priority
//  - longer quantum compared to interactive QoS
//  - higher latency on wakeup
//  -- eg used for background cpu-bound jobs inside an interactive app
//
// SCHED_QOS_BACKGROUND:
//  - dynamic priority
//  - longest quantum of all QoS
//  - higher latency on wakeup
//  -- eg used for system wide or user specific cpu-bound jobs that should run on the background
#define SCHED_QOS_REALTIME      5
#define SCHED_QOS_URGENT        4
#define SCHED_QOS_INTERACTIVE   3
#define SCHED_QOS_UTILITY       2
#define SCHED_QOS_BACKGROUND    1

#define SCHED_QOS_COUNT         5


// Priorities per QoS level
#define QOS_PRI_HIGHEST      7
#define QOS_PRI_NORMAL       0
#define QOS_PRI_LOWEST      -8

#define QOS_PRI_SHIFT       4
#define QOS_PRI_COUNT       (1 << QOS_PRI_SHIFT)


typedef struct vcpu_policy {
    int qos_class;
    int qos_priority;
} vcpu_policy_t;



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
    vcpu_policy_t           policy;
    unsigned int            flags;
    intptr_t                data;
} _vcpu_acquire_attr_t;

#endif /* _KPI_VCPU_H */
