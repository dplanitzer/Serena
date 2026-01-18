//
//  vcpu_yield.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>


void vcpu_yield(void)
{
    (void)_syscall(SC_vcpu_yield);
}
