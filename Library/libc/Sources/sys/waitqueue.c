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

    if (_syscall(SC_waq_create, 0, &q) == 0) {
        return q;
    }
    else {
        return -1;
    }
}

int waq_wait(int q)
{
    return (int)_syscall(SC_waq_wait, q, NULL);
}

int waq_timedwait(int q, int flags, const struct timespec* _Nonnull wtp)
{
    return (int)_syscall(SC_waq_timedwait, q, flags, wtp, NULL);
}

int waq_wakeup(int q, int flags)
{
    return (int)_syscall(SC_waq_wakeup, q, flags, 0);
}



int wsq_create(void)
{
    int q;

    if (_syscall(SC_waq_create, __UWQ_SIGNALLING, &q) == 0) {
        return q;
    }
    else {
        return -1;
    }
}

int wsq_wait(int q, unsigned int* _Nonnull psigs)
{
    return (int)_syscall(SC_waq_wait, q, psigs);
}

int wsq_timedwait(int q, int flags, const struct timespec* _Nonnull wtp, unsigned int* _Nonnull psigs)
{
    return (int)_syscall(SC_waq_timedwait, q, flags, wtp, psigs);
}

int wsq_signal(int q, int flags, unsigned int sigs)
{
    return (int)_syscall(SC_waq_wakeup, q, flags, sigs);
}
