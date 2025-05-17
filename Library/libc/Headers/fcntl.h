//
//  fcntl.h
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _FCNTL_H
#define _FCNTL_H 1

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <kpi/fcntl.h>

__CPP_BEGIN

// Creates an empty file at the filesystem location and with the name specified
// by 'path'. Creating a file is non-exclusive by default which means that the
// file is created if it does not exist and simply opened in it current state if
// it does exist. You may request non-exclusive behavior by passing the
// O_EXCL option. If the file already exists and you requested exclusive
// behavior, then this function will fail and return an EEXIST error. You may
// request that the newly opened file (relevant in non-exclusive mode) is
// automatically and atomically truncated to length 0 if it contained some data
// by passing the O_TRUNC option. 'permissions' are the file permissions
// that are assigned to a newly created file if it is actually created.
// @Concurrency: Safe
extern errno_t creat(const char* _Nonnull path, unsigned int mode, FilePermissions permissions, int* _Nonnull ioc);

// Opens an already existing file located at the filesystem location 'path'.
// Returns an error if the file does not exist or the caller lacks the necessary
// permissions to successfully open the file. 'mode' specifies whether the file
// should be opened for reading and/or writing. 'O_APPEND' may be passed in
// addition to 'O_WRONLY' to force the system to always append any newly written
// data to the file. The file position is disregarded by the write function(s) in
// this case.
// @Concurrency: Safe
extern errno_t open(const char* _Nonnull path, unsigned int mode, int* _Nonnull ioc);

// Performs an operation on the given descriptor. The operation is performed on
// the descriptor itself rather than the underlying resource.
// @Concurrency: Safe
extern int fcntl(int fd, int cmd, ...);

__CPP_END

#endif /* _FCNTL_H */
