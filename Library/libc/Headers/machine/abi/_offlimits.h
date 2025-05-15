//
//  _offlimits.h
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYS_OFF_LIMITS_H
#define __SYS_OFF_LIMITS_H 1

#include <machine/abi/_dmdef.h>

#ifndef OFF_MIN
#define OFF_MIN  __OFF_MIN
#endif
#ifndef OFF_MAX
#define OFF_MAX  __OFF_MAX
#endif
#ifndef OFF_WIDTH
#define OFF_WIDTH __OFF_WIDTH
#endif

#endif /* __SYS_OFF_LIMITS_H */
