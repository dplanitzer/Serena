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

int wq_sigwait(int q, const sigset_t* _Nullable mask, sigset_t* _Nonnull sigs)
{
    return (int)_syscall(SC_wq_sigwait, q, mask, sigs);
}

int wq_timedwait(int q, int flags, const struct timespec* _Nonnull wtp)
{
    return (int)_syscall(SC_wq_timedwait, q, flags, wtp);
}

int wq_sigtimedwait(int q, const sigset_t* _Nullable mask, sigset_t* _Nonnull sigs, int flags, const struct timespec* _Nonnull wtp)
{
    return (int)_syscall(SC_wq_sigtimedwait, q, mask, sigs, flags, wtp);
}

int wq_wakeup(int q, int flags)
{
    return (int)_syscall(SC_wq_wakeup, q, flags);
}
