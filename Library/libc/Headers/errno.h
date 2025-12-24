//
//  errno.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _ERRNO_H
#define _ERRNO_H 1

#if ___STDC_HOSTED__ != 1
#error "not supported in freestanding mode"
#endif

#include <_cmndef.h>
#include <kpi/_errno.h>

__CPP_BEGIN

extern _Errno_t* _Nonnull __vcpu_errno(void);

#define errno (*__vcpu_errno())

__CPP_END

#endif /* _ERRNO_H */
