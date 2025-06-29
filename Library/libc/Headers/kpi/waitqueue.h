//
//  kpi/waitqueue.h
//  libc
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_WAITQUEUE_H
#define _KPI_WAITQUEUE_H 1

#include <_cmndef.h>

__CPP_BEGIN

// FIFO wait queue policy
#define WAITQUEUE_FIFO  0

// Specify the default wait queue policy
#define WAITQUEUE_DEFAULT   WAITQUEUE_FIFO


// Requests that wakeup() wakes up all vcpus on the wait queue.
#define WAKE_ALL    0

// Requests that wakeup() wakes up at most one vcpu instead of all.
#define WAKE_ONE    1

__CPP_END

#endif /* _KPI_WAITQUEUE_H */
