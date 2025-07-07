//
//  signal.c
//  libc
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <signal.h>
#include <kpi/syscall.h>


int sigwait(const sigset_t* _Nonnull set, siginfo_t* _Nullable info)
{
    return (int)_syscall(SC_sigwait, set, info);
}

int sigtimedwait(const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, siginfo_t* _Nullable info)
{
    return (int)_syscall(SC_sigtimedwait, set, flags, wtp, info);
}

int sigpending(sigset_t* _Nonnull set)
{
    return (int)_syscall(SC_sigpending, set);
}
