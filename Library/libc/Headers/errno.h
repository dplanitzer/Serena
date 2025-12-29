//
//  errno.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <_cmndef.h>
#include <_errno.h>

#ifndef _ERRNO_H
#define _ERRNO_H 1

#if ___STDC_HOSTED__ != 1
#error "not supported in freestanding mode"
#endif

__CPP_BEGIN

extern int* _Nonnull __vcpu_errno(void);

#define errno (*__vcpu_errno())

__CPP_END

#endif /* _ERRNO_H */
