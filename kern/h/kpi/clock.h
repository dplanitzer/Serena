//
//  kpi/clock.h
//  kpi
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_CLOCK_H
#define _KPI_CLOCK_H 1

#include <kpi/_time.h>

// Clock information
#define CLOCK_INFO_BASIC    1

typedef void* clock_info_ref;

typedef struct clock_basic_info {
    struct timespec tick_resolution;    // length of a single clock tick in terms of seconds and nanoseconds
} clock_basic_info_t;

#endif /* _KPI_CLOCK_H */
