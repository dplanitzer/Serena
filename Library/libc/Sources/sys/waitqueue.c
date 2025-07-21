//
//  waitqueue.c
//  libc
//
//  Created by Dietmar Planitzer on 6/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/waitqueue.h>
#include <kpi/syscall.h>


int wq_create(int policy)
{
    int q;

    if (_syscall(SC_wq_create, policy, &q) == 0) {
        return q;
    }
    else {
        return -1;
    }
}

int wq_wait(int q)
{
    return (int)_syscall(SC_wq_wait, q);
}

int wq_timedwait(int q, int flags, const struct timespec* _Nonnull wtp)
{
    return (int)_syscall(SC_wq_timedwait, q, flags, wtp);
}

int wq_wakeup_then_timedwait(int q, int q2, int flags, const struct timespec* _Nonnull wtp)
{
    return (int)_syscall(SC_wq_wakeup_then_timedwait, q, q2, flags, wtp);
}

int wq_wakeup(int q, int flags)
{
    return (int)_syscall(SC_wq_wakeup, q, flags);
}
