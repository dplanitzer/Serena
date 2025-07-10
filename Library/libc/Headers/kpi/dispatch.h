//
//  kpi/dispatch.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_DISPATCH_H
#define _KPI_DISPATCH_H 1

#include <_cmndef.h>

typedef void (*os_dispatch_func_t)(void* _Nullable arg);

// Quality of Service level. From highest to lowest.
// kDispatchQoS_Realtime: kernel will minimize the scheduling latency. Realtime is always scheduled before anything else
// kDispatchQoS_Idle: no guarantee with regards to schedule latency. Only scheduled if there is nothing to schedule for a DISPATCH_QOS_XXX > kDispatchQoS_Idle
#define kDispatchQoS_Realtime       4
#define kDispatchQoS_Interactive    3
#define kDispatchQoS_Utility        2
#define kDispatchQoS_Background     1
#define kDispatchQoS_Idle           0

#define kDispatchQoS_Count          5


// Priorities per QoS level
#define kDispatchPriority_Highest   5
#define kDispatchPriority_Normal    0
#define kDispatchPriority_Lowest   -6

#define kDispatchPriority_Count     12


// Private
enum {
    kDispatchOption_Sync = 1,       // Dispatch and then wait for completion
    kDispatchOption_Coalesce = 2,   // Do not dispatch this request if a request with the same tag is already queued or currently executing
};

#endif /* _KPI_DISPATCH_H */
