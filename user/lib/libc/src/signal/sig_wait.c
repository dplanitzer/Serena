//
//  sig_wait.c
//  libc
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/signal.h>

int sig_wait(const sigset_t* _Nonnull set, int* _Nonnull signo)
{
    return (int)_syscall(SC_sig_wait, set, signo);
}
