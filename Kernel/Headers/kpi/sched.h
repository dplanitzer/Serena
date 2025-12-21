//
//  kpi/sched.h
//  libc
//
//  Created by Dietmar Planitzer on 10/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_SCHED_H
#define _KPI_SCHED_H 1

#include <kpi/types.h>

// Quality of Service level. From highest to lowest.
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
//
// SCHED_QOS_IDLE:
//  - fixed priority
//  - shortest quantum of all QoS
//  - may put CPU temporarily to sleep
//  - used by the scheduler to consume unused CPU cycles
#define SCHED_QOS_REALTIME      5
#define SCHED_QOS_URGENT        4
#define SCHED_QOS_INTERACTIVE   3
#define SCHED_QOS_UTILITY       2
#define SCHED_QOS_BACKGROUND    1
#define SCHED_QOS_IDLE          0

#define SCHED_QOS_COUNT         6


// Priorities per QoS level
#define QOS_PRI_HIGHEST      7
#define QOS_PRI_NORMAL       0
#define QOS_PRI_LOWEST      -8

#define QOS_PRI_SHIFT       4
#define QOS_PRI_COUNT       (1 << QOS_PRI_SHIFT)


#define SCHED_PARAM_QOS     1

struct sched_qos_params {
    int category;
    int priority;
};

typedef struct sched_params {
    int type;
    union {
        struct sched_qos_params qos;
    }   u;
} sched_params_t;

#endif /* _KPI_SCHED_H */
