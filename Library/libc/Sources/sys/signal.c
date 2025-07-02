//
//  signal.c
//  libc
//
//  Created by Dietmar Planitzer on 6/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/signal.h>
#include <kpi/syscall.h>


int sig_wait(sigset_t* _Nullable sigs)
{
    return (int)_syscall(SC_sig_wait, sigs);
}

int sig_timedwait(int flags, const struct timespec* _Nonnull wtp, sigset_t* _Nullable sigs)
{
    return (int)_syscall(SC_sig_timedwait, flags, wtp, sigs);
}

int sig_raise(int vcpuid, int signo)
{
    return (int)_syscall(SC_sig_raise, vcpuid, signo);
}

sigset_t sig_getmask(void)
{
    sigset_t mask;

    (void)_syscall(SC_sig_setmask, SIG_BLOCK, 0, &mask);
    return mask;
}

int sig_setmask(int op, sigset_t mask, sigset_t* _Nullable oldmask)
{
    return (int)_syscall(SC_sig_setmask, op, mask, oldmask);
}
