//
//  kpi/host.h
//  kpi
//
//  Created by Dietmar Planitzer on 3/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_HOST_H
#define _KPI_HOST_H 1

#include <_cmndef.h>
#include <ext/timespec.h>
#include <kpi/types.h>


typedef struct proc_matcher {
    int flavor;
    int value;
} proc_matcher_t;

#define _PROC_MATCH_PPID    1
#define _PROC_MATCH_SID     2
#define _PROC_MATCH_UID     3
#define _PROC_MATCH_END     -1

#define PROC_MATCHING_PPID(__ppid) \
(proc_matcher_t){_PROC_MATCH_PPID, (int)(__ppid)}

#define PROC_MATCHING_SID(__sid) \
(proc_matcher_t){_PROC_MATCH_SID, (int)(__sid)}

#define PROC_MATCHING_UID(__uid) \
(proc_matcher_t){_PROC_MATCH_UID, (int)(__uid)}

#define PROC_MATCHING_END() \
(proc_matcher_t){_PROC_MATCH_END, 0}

#endif /* _KPI_HOST_H */
