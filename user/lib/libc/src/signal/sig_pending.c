//
//  sig_pending.c
//  libc
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/signal.h>

int sig_pending(sigset_t* _Nonnull set)
{
    return (int)_syscall(SC_sig_pending, set);
}
