//
//  sig_route.c
//  libc
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/signal.h>

int sig_route(int op, int signo, int scope, id_t id)
{
    return (int)_syscall(SC_sig_route, op, signo, scope, id);
}
