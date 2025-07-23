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

#define JOIN_PROC       0   /* Join child process with pid */
#define JOIN_PROC_GROUP 1   /* Join any member of the child process group */
#define JOIN_ANY        2   /* Join any child process */


#define JREASON_EXIT       0
#define JREASON_SIGNALED   1


// The result of a waitpid() system call.
struct proc_status {
    pid_t   pid;        // pid of the child process
    int     reason;     // termination reason
    union {
        int status;     // child process exit status
        int signo;      // signal that caused the process to terminate
    }       u;
};

#endif /* _KPI_WAIT_H */
