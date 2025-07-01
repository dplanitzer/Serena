//
//  signal.c
//  libc
//
//  Created by Dietmar Planitzer on 6/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/signal.h>
#include <kpi/syscall.h>


int sig_wait(unsigned int* _Nullable psigs)
{
    return (int)_syscall(SC_sig_wait, psigs);
}

int sig_timedwait(int flags, const struct timespec* _Nonnull wtp, unsigned int* _Nullable psigs)
{
    return (int)_syscall(SC_sig_timedwait, flags, wtp, psigs);
}

int sig_raise(int vcpuid, int signo)
{
    return (int)_syscall(SC_sig_raise, vcpuid, signo);
}

unsigned int sig_getmask(void)
{
    unsigned int mask;

    (void)_syscall(SC_sig_setmask, SIGMASK_OP_ENABLE, 0, &mask);
    return mask;
}

int sig_setmask(int op, unsigned int mask, unsigned int* _Nullable oldmask)
{
    return (int)_syscall(SC_sig_setmask, op, mask, oldmask);
}
