//
//  process.h
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_PROCESS_H
#define _SYS_PROCESS_H 1

#include <inttypes.h>
#include <stdnoreturn.h>
#include <kpi/_access.h>
#include <kpi/_time.h>
#include <kpi/process.h>
#include <serena/types.h>

__CPP_BEGIN

// Checks whether the file at the filesystem location 'path' exists and whether
// it is accessible according to 'mode'. A suitable error is returned otherwise.
// @Concurrency: Safe
extern int access(const char* _Nonnull path, int mode);

extern int chdir(const char* _Nonnull path);
extern int getcwd(char* _Nonnull buffer, size_t bufferSize);

extern int chown(const char* _Nonnull path, uid_t uid, gid_t gid);

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


// Truncates the file at the filesystem location 'path'. If the new length is
// greater than the size of the existing file, then the file is expanded and the
// newly added data range is zero-filled. If the new length is less than the
// size of the existing file, then the excess data is removed and the size of
// the file is set to the new length.
// @Concurrency: Safe
extern int truncate(const char* _Nonnull path, off_t length);


// Deletes the file located at the filesystem location 'path'.
// @Concurrency: Safe
extern int unlink(const char* _Nonnull path);

// Deletes the empty directory located at the filesystem location 'path'.
// Note that this function deletes empty directories only.
// @Concurrency: Safe
extern int rmdir(const char* _Nonnull path);


// Synchronously writes all dirty disk blocks back to disk.
extern void sync(void);

__CPP_END

#endif /* _SYS_PROCESS_H */
