//
//  woa_wait.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/synch.h>
#include <kpi/syscall.h>


int woa_wait(volatile atomic_int* _Nonnull addr, int expected, int flags, const nanotime_t* _Nullable wtp)
{
    return (int)_syscall(SC_woa_wait, addr, expected, flags, wtp);
}
