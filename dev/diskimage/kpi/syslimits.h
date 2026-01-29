//
//  kpi/syslimits.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#if defined(_WIN32) || defined(_WIN64)
#include <../../kern/h/kpi/syslimits.h>

#define NLINK_MAX ((int)0x7fffffff)

#endif

#if defined(__APPLE__)
#include <sys/syslimits.h>
#endif
