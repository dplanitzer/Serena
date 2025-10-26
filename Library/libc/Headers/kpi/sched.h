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
// SCHED_QOS_REALTIME: kernel will minimize the scheduling latency. Realtime is always scheduled before anything else
// SCHED_QOS_BACKGROUND: no guarantee with regards to schedule latency.
// SCHED_QOS_IDLE: kernel internal. Has only 1 priority. Only scheduled if there is nothing to schedule for a SCHED_QOS_XXX > SCHED_QOS_IDLE
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
