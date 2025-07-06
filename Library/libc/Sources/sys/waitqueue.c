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

int wq_wait(int q, const sigset_t* _Nullable mask)
{
    return (int)_syscall(SC_wq_wait, q, mask);
}

int wq_timedwait(int q, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    return (int)_syscall(SC_wq_timedwait, q, mask, flags, wtp);
}

int wq_timedwakewait(int q, int oq, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    return (int)_syscall(SC_wq_timedwakewait, q, oq, mask, flags, wtp);
}

int wq_wakeup(int q, int flags)
{
    return (int)_syscall(SC_wq_wakeup, q, flags);
}
