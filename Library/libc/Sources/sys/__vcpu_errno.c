//
//  __vcpu_errno.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <ext/try.h>
#include <kpi/syscall.h>


errno_t* _Nonnull __vcpu_errno(void)
{
    return (errno_t*)_syscall(SC_vcpu_errno);
}
