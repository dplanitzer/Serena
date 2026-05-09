//
//  kpi/synch.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_SYNCH_H
#define _KPI_SYNCH_H 1

#include <_cmndef.h>

#define WAKEUP_ALL  0   /* Requests that wakeup() wakes up all vcpus on the wait queue. */
#define WAKEUP_ONE  1   /* Requests that wakeup() wakes up at most one vcpu instead of all. */

#if defined(__KERNEL__)
#define _WAKEUP_UMASK   (WAKEUP_ALL | WAKEUP_ONE)
#endif

#endif /* _KPI_SYNCH_H */
