//
//  sig_send.c
//  libc
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/signal.h>

int sig_send(int scope, id_t id, int signo)
{
    return (int)_syscall(SC_sig_send, scope, id, signo);
}
