//
//  waitqueue.c
//  libc
//
//  Created by Dietmar Planitzer on 6/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/waitqueue.h>
#include <kpi/syscall.h>


int waq_create(void)
{
    int q;

    if (_syscall(SC_waq_create, &q) == 0) {
        return q;
    }
    else {
        return -1;
    }
}

int waq_wait(int q)
{
    return (int)_syscall(SC_waq_wait, q);
}

int waq_timedwait(int q, int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp)
{
    return (int)_syscall(SC_waq_timedwait, q, flags, wtp, rmtp);
}

int waq_wakeup(int q, int flags)
{
    return (int)_syscall(SC_waq_wakeup, q, flags);
}
