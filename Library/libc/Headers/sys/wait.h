//
//  sys/wait.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H 1

#include <kern/_cmndef.h>
#ifdef __KERNEL__
#include <kern/errno.h>
#include <kern/types.h>
#else
#include <sys/errno.h>
#include <sys/types.h>
#endif

__CPP_BEGIN

// The result of a waitpid() system call.
typedef struct pstatus {
    pid_t   pid;        // PID of the child process
    int     status;     // Child process exit status
} pstatus_t;


extern errno_t waitpid(pid_t pid, pstatus_t* _Nullable result);

__CPP_END

#endif /* _SYS_WAIT_H */
