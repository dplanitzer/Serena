//
//  __vcpu_errno.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>


int* _Nonnull __vcpu_errno(void)
{
    return (int*)_syscall(SC_vcpu_errno);
}
