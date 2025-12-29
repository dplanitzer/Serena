//
//  _errno.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/_errno.h>

#if __STDC_WANT_LIB_EXT1__ == 1 && __ERRNO_T_DEFINED != 1
#define __ERRNO_T_DEFINED 1
typedef int errno_t;
#endif

#ifndef EOK
#define EOK _EOK
#endif
