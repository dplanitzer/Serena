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
#include <ext/timespec.h>
#include <kpi/types.h>
#include <stdint.h>

#define VCPUID_SELF 0
#define VCPUID_MAIN 1

#define VCPUID_MAIN_GROUP   1


struct vcpu;
typedef struct vcpu* vcpu_t;


// Quality of Service level/class/grade. From highest to lowest.
// Note that the highest priority of Realtime and the lowest priority of
// Background are privileged priority bands that owned by the scheduler.
// 
// VCPU_QOS_REALTIME:
//  - fixed priority
//  - smallest quantum
//  - minimized latency on wakeup
//  -- eg used for animations, video  & audio playback
//
// VCPU_QOS_URGENT:
//  - dynamic priority
//  - longer quantum compared to realtime QoS
//  - short latency on wakeup
//  -- eg used for I/O drivers
//
// VCPU_QOS_INTERACTIVE:
//  - dynamic priority
//  - longer quantum compared to urgent QoS
//  - short latency on wakeup
//  -- eg used for apps the user is interacting with
//
// VCPU_QOS_UTILITY:
//  - dynamic priority
//  - longer quantum compared to interactive QoS
//  - higher latency on wakeup
//  -- eg used for background cpu-bound jobs inside an interactive app
//
// VCPU_QOS_BACKGROUND:
//  - dynamic priority
//  - longest quantum of all QoS
//  - higher latency on wakeup
//  -- eg used for system wide or user specific cpu-bound jobs that should run on the background
#define VCPU_QOS_REALTIME       5
#define VCPU_QOS_URGENT         4
#define VCPU_QOS_INTERACTIVE    3
#define VCPU_QOS_UTILITY        2
#define VCPU_QOS_BACKGROUND     1

#define VCPU_QOS_COUNT          5


// Priorities per QoS level
#define VCPU_PRI_HIGHEST    7
#define VCPU_PRI_NORMAL     0
#define VCPU_PRI_LOWEST    -8

#define VCPU_PRI_SHIFT      4
#define VCPU_PRI_COUNT      (1 << VCPU_PRI_SHIFT)


typedef struct vcpu_qos {
    int grade;
    int priority;
} vcpu_qos_t;

typedef struct vcpu_policy {
    int         version;        // sizeof(vcpu_policy_t)
    vcpu_qos_t  qos;
} vcpu_policy_t;


// Acquire the VP and immediately resume it
#define VCPU_ACQUIRE_RESUMED    1

typedef void (*vcpu_func_t)(void* _Nullable);

typedef struct _vcpu_acquire_attr {
    vcpu_func_t _Nullable   func;
    void* _Nullable         arg;
    size_t                  stack_size;
    vcpuid_t                group_id;
    vcpu_policy_t           policy;
    unsigned int            flags;
    intptr_t                data;
} _vcpu_acquire_attr_t;


// VCPU specific state. The actual state records are defined in the architecture
// specific header files.
typedef void* vcpu_state_ref;


// Information about a VCPU
typedef void* vcpu_info_ref;

#define VCPU_INFO_STACK         1
#define VCPU_INFO_SCHEDULING    2
#define VCPU_INFO_IDS           3
#define VCPU_INFO_TIMES         4


typedef struct vcpu_stack_info {
    void*   base_ptr;
    size_t  size;
} vcpu_stack_info_t;


#define VCPU_RUST_INITIATED     0   /* VP was just created and has not been scheduled yet */
#define VCPU_RUST_READY         1   /* VP is able to run and is currently sitting on the ready queue */
#define VCPU_RUST_RUNNING       2   /* VP is running */
#define VCPU_RUST_WAITING       3   /* VP is blocked waiting for a resource (eg sleep, mutex, semaphore, etc) */
#define VCPU_RUST_SUSPENDED     4   /* VP was running or ready and is now suspended (it is not on any queue) */
#define VCPU_RUST_TERMINATING   5   /* VP is in the process of terminating and being reaped (it's on the finalizer queue) */

#define VCPU_HAS_PRIORITY_BOOST     0x01
#define VCPU_HAS_QUANTUM_BOOST      0x02
#define VCPU_HAS_PRIORITY_PENALTY   0x04

typedef struct vcpu_scheduling_info {
    int             run_state;
    vcpu_qos_t      base_qos;
    vcpu_qos_t      cur_qos;
    int             base_quantum_length;
    int             cur_quantum_length;
    int             suspend_count;
    unsigned int    flags;
} vcpu_scheduling_info_t;


typedef struct vcpu_ids_info {
    vcpuid_t    id;
    vcpuid_t    group_id;
} vcpu_ids_info_t;


typedef struct vcpu_times_info {
    struct timespec acquisition_time;   // Time when the process acquired this vcpu
    
    struct timespec user_time;          // Time the vcpu has spent running in user mode since acquisition
    struct timespec system_time;        // Time the vcpu has spent running in system/kernel mode since acquisition
    struct timespec wait_time;          // Time the vcpu has spent in waiting or suspended state since acquisition
} vcpu_times_info_t;

#endif /* _KPI_VCPU_H */
