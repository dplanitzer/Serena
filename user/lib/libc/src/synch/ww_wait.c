//
//  ww_wait.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/synch.h>
#include <kpi/syscall.h>


int ww_wait(volatile atomic_int* _Nonnull addr, int expected)
{
    return (int)_syscall(SC_ww_wait, addr, expected);
}
