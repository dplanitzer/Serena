//
//  unistd.h
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <inttypes.h>
#include <sys/types.h>

__CPP_BEGIN

// Standard I/O channels that are open when a process starts. These channels
// connect to the terminal input and output streams.
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2


// Checks whether the file at the filesystem location 'path' exists and whether
// it is accessible according to 'mode'. A suitable error is returned otherwise.
// @Concurrency: Safe
extern int access(const char* _Nonnull path, int mode);

extern int chdir(const char* _Nonnull path);

__CPP_END

#endif /* _UNISTD_H */
