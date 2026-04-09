//
//  sig_timedwait.c
//  libc
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/signal.h>

int sig_timedwait(const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nonnull signo)
{
    return (int)_syscall(SC_sig_timedwait, set, flags, wtp, signo);
}
