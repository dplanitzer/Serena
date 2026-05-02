//
//  kpi/wait.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/02/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_WAIT_H
#define _KPI_WAIT_H 1

#include <_cmndef.h>
#include <stdint.h>
#include <kpi/types.h>

// proc_waitstate() state to wait for
#define WAIT_FOR_ANY        0
#define WAIT_FOR_RESUMED    1
#define WAIT_FOR_SUSPENDED  2
#define WAIT_FOR_TERMINATED 3

// proc_waitstate() match type
#define WAIT_PID    0   /* Status of child process with pid */
#define WAIT_GROUP  1   /* Status of any member of the specified child process group */
#define WAIT_ANY    2   /* Status of any of the process' child processes */

// proc_waitstate() flags
#define WAIT_NONBLOCKING    1   /* Do not block waiting for a status change. Return EAGAIN if no status change found.*/

// Reason for a status change
#define WAIT_REASON_EXITED      1
#define WAIT_REASON_SIGNALED    2
#define WAIT_REASON_EXCEPTION   3


// Result of a proc_waitstate() call.
typedef struct proc_waitres {
    pid_t   pid;        // pid of the child process
    int     state;      // new process state (see PROC_STATE_XXX)
    int     reason;     // reason for state change
    union {
        int status;     // child process exit status
        int signo;      // signal that caused the process to terminate
        int excptno;    // exception that caused the process to terminate
    }       u;
    intptr_t    reserved[5];
} proc_waitres_t;

#endif /* _KPI_WAIT_H */
