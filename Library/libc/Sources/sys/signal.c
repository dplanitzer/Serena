//
//  signal.c
//  libc
//
//  Created by Dietmar Planitzer on 6/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/signal.h>
#include <kpi/syscall.h>


int sig_wait(const sigset_t* _Nullable mask, sigset_t* _Nonnull sigs)
{
    return (int)_syscall(SC_sig_wait, mask, sigs);
}

int sig_timedwait(const sigset_t* _Nullable mask, sigset_t* _Nonnull sigs, int flags, const struct timespec* _Nonnull wtp)
{
    return (int)_syscall(SC_sig_timedwait, mask, sigs, flags, wtp);
}

int sig_raise(vcpuid_t vcpuid, int signo)
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
