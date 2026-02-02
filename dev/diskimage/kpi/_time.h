//
//  _time.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _DI_PRIV_TIME_H
#define _DI_PRIV_TIME_H 1

#include <time.h>

// Time conversion factors
#define NSEC_PER_SEC    1000000000l
#define USEC_PER_SEC    1000000l
#define MSEC_PER_SEC    1000l

typedef long mseconds_t;
typedef long useconds_t;

#endif /* _DI_PRIV_TIME_H */
