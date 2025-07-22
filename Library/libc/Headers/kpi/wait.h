//
//  kpi/wait.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_WAIT_H
#define _KPI_WAIT_H 1

#include <kpi/types.h>

#define WNOHANG 1

#define _WSTATUSMASK    0x00ffffff
#define _WREASONMASK    0xff000000

#define _WNORMTERM      0
#define _WSIGNALED      (1 << 24)

#define WMAKEEXITED(__status) \
(((__status) & _WSTATUSMASK) | _WNORMTERM)

#define WMAKESIGNALED(__signo) \
(((__signo) & _WSTATUSMASK) | _WSIGNALED)


// The result of a waitpid() system call.
struct _pstatus {
    pid_t   pid;        // PID of the child process
    int     status;     // Child process exit status
};

#endif /* _KPI_WAIT_H */
