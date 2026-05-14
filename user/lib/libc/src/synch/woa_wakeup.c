//
//  woa_wakeup.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/synch.h>
#include <kpi/syscall.h>


int woa_wakeup(volatile atomic_int* _Nonnull addr, int flags)
{
    return (int)_syscall(SC_woa_wakeup, addr, flags);
}
