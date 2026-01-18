//
//  _time.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <time.h>

// Time conversion factors
#define NSEC_PER_SEC    1000000000l
#define USEC_PER_SEC    1000000l
#define MSEC_PER_SEC    1000l

#if defined(_WIN32) || defined(_WIN64)
typedef long mseconds_t;
typedef long useconds_t;
#endif
