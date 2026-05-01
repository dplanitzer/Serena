//
//  proc_terminate.c
//  libc
//
//  Created by Dietmar Planitzer on 4/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <serena/signal.h>


int proc_terminate(pid_t pid)
{
    return sig_send(SIG_SCOPE_PROC, pid, SIG_TERMINATE);
}
