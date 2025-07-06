//
//  sched.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"


SYSCALL_0(sched_yield)
{
    VirtualProcessor_Yield();
    return EOK;
}
