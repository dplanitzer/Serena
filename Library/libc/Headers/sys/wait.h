//
//  sys/wait.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H 1

#include <_cmndef.h>
#include <ext/timespec.h>
#include <kpi/wait.h>
#include <sys/types.h>

__CPP_BEGIN

// Checks whether the specified child process or a member of the specified child
// process group has terminated and is available for joining/reaping. If so,
// then this function fills out the process status record 'ps' and returns 0.
// Otherwise it blocks until the specified process has terminated or a member of
// the specified child process group has terminated. A 'scope' of JOIN_PROC
// indicates that the function should wait until the process with PID 'id' has
// terminated. A 'scope' of JOIN_PROC_GROUP indicates that the function should
// wait until a member of the child process group with id 'id' has terminated
// and a 'scope' of JOIN_ANY indicates that this function should wait until any
// child process terminates. You should pass 0 for 'id' if the scope is JOIN_ANY.  
extern int proc_join(int scope, pid_t id, struct proc_status* _Nonnull ps);

// Similar to proc_join() but imposes a timeout of 'wtp' on the wait. If 'wtp'
// is 0 and (or a absolute time in the past) then this function simply checks
// whether the indicated process has terminated without ever waiting. This
// function returns ETIMEDOUT in this case or if the timeout has been reached
// without the indicated process terminating. 
extern int proc_timedjoin(int scope, pid_t id, int flags, const struct timespec* _Nonnull wtp, struct proc_status* _Nonnull ps);

__CPP_END

#endif /* _SYS_WAIT_H */
