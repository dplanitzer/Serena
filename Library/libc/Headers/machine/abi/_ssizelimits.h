//
//  _ssizelimits.h
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYS_SSIZE_LIMITS_H
#define __SYS_SSIZE_LIMITS_H 1

#include <machine/abi/_dmdef.h>

#ifndef SSIZE_MIN
#define SSIZE_MIN  __SSIZE_MIN
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX  __SSIZE_MAX
#endif
#ifndef SSIZE_WIDTH
#define SSIZE_WIDTH __SSIZE_WIDTH
#endif

#endif /* __SYS_SSIZE_LIMITS_H */
