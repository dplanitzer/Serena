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

#define _WSTATUSMASK    0x0000ffff
#define _WSIGNUMMASK    0x00ff0000
#define _WREASONMASK    0x0f000000

#define _WNORMTERM      0x00
#define _WSIGNALED      0x01


// The result of a waitpid() system call.
struct _pstatus {
    pid_t   pid;        // PID of the child process
    int     status;     // Child process exit status
};

#endif /* _KPI_WAIT_H */
