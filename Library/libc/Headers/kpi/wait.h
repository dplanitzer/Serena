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

// The result of a waitpid() system call.
typedef struct pstatus {
    pid_t   pid;        // PID of the child process
    int     status;     // Child process exit status
} pstatus_t;

#endif /* _KPI_WAIT_H */
