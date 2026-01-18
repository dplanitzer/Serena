//
//  vcpu_uret.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/20/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "vcpu.h"
#include <kpi/syscall.h>


void vcpu_uret_relinquish_self(void)
{
    _syscall(SC_vcpu_relinquish_self);
    /* NOT REACHED */
}

void vcpu_uret_exit(void)
{
    _syscall(SC_exit, 0);
    /* NOT REACHED */
}
