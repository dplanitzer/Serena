//
//  process.h
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_PROCESS_H
#define _SERENA_PROCESS_H 1

#include <stdnoreturn.h>
#include <kpi/process.h>

__CPP_BEGIN

// Gets/sets the current working directory of the process.
extern int proc_cwd(char* _Nonnull buffer, size_t bufferSize);
extern int proc_setcwd(const char* _Nonnull path);

// Returns the process id of the calling process.
extern pid_t proc_self(void);

// Exits the current process. This call is different from exit() in the sense
// that it does not invoke atexit() callbacks.
extern _Noreturn void proc_exit(int status);


// Replaces the currently executing process image with the executable image stored
// at 'path'. All open I/O channels except channels 0, 1 and 2 are closed.
extern int proc_exec(const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable * _Nullable envp);

extern pargs_t* _Nonnull getpargs(void);


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


// Sets the process' umask. Bits set in this mask are cleared in the permissions
// that are used to create a file. Returns the old umask. Note that calling this
// function with SEO_UMASK_NO_CHANGE as the argument causes umask() to simply
// return the current umask without chaning it as a side-effect.
// @Concurrency: Safe
extern mode_t umask(mode_t mask);


extern int proc_schedparam(pid_t pid, int type, int* _Nonnull param);
extern int proc_setschedparam(pid_t pid, int type, const int* _Nonnull param);


extern int proc_info(pid_t pid, int flavor, proc_info_ref _Nonnull info);
extern int proc_name(pid_t pid, char* _Nonnull buf, size_t bufSize);

// Fills the buffer with an array of vcpuid_t's of all currently acquired vcpus
// in the calling process. 'bufSize' is the size of the buffer in terms of the
// number of vcpuid_t objects that it can hold. 'buf' will be terminated by a
// 0 vcpuid_t. Thus 'bufSize' is expected to be equal to the number of vcpuid_t's
// that should be returned plus 1. Returns 0 on success and if the buffer is
// big enough to hold the vcpu ids of all acquired vcpus. Returns 1 if the
// buffer isn't big enough to hold the ids of all acquired vcpus. However the
// buffer is still filled with as many ids as fit. Returns -1 on an error.
extern int proc_vcpus(vcpuid_t* _Nonnull buf, size_t bufSize);

__CPP_END

#endif /* _SERENA_PROCESS_H */
