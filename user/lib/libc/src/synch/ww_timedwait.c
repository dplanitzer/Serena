//
//  ww_timedwait.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/synch.h>
#include <kpi/syscall.h>


int ww_timedwait(volatile atomic_int* _Nonnull addr, int expected, int flags, const nanotime_t* _Nonnull wtp)
{
    return (int)_syscall(SC_ww_timedwait, addr, expected, flags, wtp);
}
