//
//  signal.c
//  libc
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <signal.h>
#include <kpi/syscall.h>


int sigwait(const sigset_t* _Nonnull set, int* _Nonnull signo)
{
    return (int)_syscall(SC_sigwait, set, signo);
}

int sigtimedwait(const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nonnull signo)
{
    return (int)_syscall(SC_sigtimedwait, set, flags, wtp, signo);
}

int sigpending(sigset_t* _Nonnull set)
{
    return (int)_syscall(SC_sigpending, set);
}

int sigsend(int scope, id_t id, int signo)
{
    return (int)_syscall(SC_sigsend, scope, id, signo);
}
