//
//  process.h
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_PROCESS_H
#define _SYS_PROCESS_H 1

#include <stdnoreturn.h>
#include <kpi/_time.h>
#include <kpi/process.h>
#include <serena/types.h>

__CPP_BEGIN

extern int chdir(const char* _Nonnull path);
extern int getcwd(char* _Nonnull buffer, size_t bufferSize);

extern _Noreturn void _exit(int status);

extern uid_t getuid(void);
extern gid_t getgid(void);

extern pid_t getpid(void);
extern pid_t getppid(void);
extern pid_t getpgrp(void);
extern pid_t getsid(void);


// Replaces the currently executing process image with the executable image stored
// at 'path'. All open I/O channels except channels 0, 1 and 2 are closed.
extern int proc_exec(const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable * _Nullable envp);

extern pargs_t* _Nonnull getpargs(void);


// Sets the process' umask. Bits set in this mask are cleared in the permissions
// that are used to create a file. Returns the old umask. Note that calling this
// function with SEO_UMASK_NO_CHANGE as the argument causes umask() to simply
// return the current umask without chaning it as a side-effect.
// @Concurrency: Safe
extern mode_t umask(mode_t mask);


// Synchronously writes all dirty disk blocks back to disk.
extern void sync(void);

__CPP_END

#endif /* _SYS_PROCESS_H */
