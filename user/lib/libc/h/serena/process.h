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
#include <kpi/types.h>

__CPP_BEGIN

// Returns the process id of the calling process.
extern pid_t proc_self(void);

// Returns the process context. This data structure holds pointers to the
// command line arguments, environment variables, etc.
const proc_ctx_t* _Nonnull proc_context(void);


// Gets/sets the current working directory of the process.
extern int proc_cwd(char* _Nonnull buffer, size_t bufferSize);
extern int proc_setcwd(const char* _Nonnull path);

// Returns the name of the process. This is just the name without the path
// prefix.
extern int proc_name(char* _Nonnull buf, size_t bufSize);


// Sets the process' umask. Get the current umask value by calling proc_info()
// with the PROC_INFO_BASIC selector.
// @Concurrency: Safe
extern void proc_setumask(fs_perms_t umask);


extern int proc_schedparam(pid_t pid, int type, int* _Nonnull param);
extern int proc_setschedparam(pid_t pid, int type, const int* _Nonnull param);


extern int proc_info(pid_t pid, int flavor, proc_info_ref _Nonnull info);
extern int proc_property(pid_t pid, int flavor, char* _Nonnull buf, size_t bufSize);

// Fills the buffer with an array of vcpuid_t's of all currently acquired vcpus
// in the calling process. 'bufSize' is the size of the buffer in terms of the
// number of vcpuid_t objects that it can hold. 'buf' will be terminated by a
// 0 vcpuid_t. Thus 'bufSize' is expected to be equal to the number of vcpuid_t's
// that should be returned plus 1. Returns 0 on success and if the buffer is
// big enough to hold the vcpu ids of all acquired vcpus. Returns 1 if the
// buffer isn't big enough to hold the ids of all acquired vcpus. However the
// buffer is still filled with as many ids as fit. Returns -1 on an error.
extern int proc_vcpus(vcpuid_t* _Nonnull buf, size_t bufSize);


// Replaces the currently executing process image with the executable image stored
// at 'path'. All open I/O channels except channels 0, 1 and 2 are closed.
extern int proc_exec(const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable envp[]);

// Exits the current process. This call is different from exit() in the sense
// that it does not invoke atexit() callbacks.
extern _Noreturn void proc_exit(int status);


// Checks whether the specified child process or a member of the specified child
// process group has undergone a status change. If so, then this function fills 
// in the status record 'status' and returns 0. Otherwise EAGAIN is returned
// if STATUS_NONBLOCKING is set in 'flags' or it waits until a process or the
// specified process changes its state again.  
extern int proc_status(int match, pid_t id, int flags, proc_status_t* _Nonnull status);

extern int proc_terminate(pid_t pid);
extern int proc_suspend(pid_t pid);
extern int proc_resume(pid_t pid);

__CPP_END

#endif /* _SERENA_PROCESS_H */
