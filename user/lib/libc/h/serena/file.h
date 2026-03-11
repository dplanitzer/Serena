//
//  file.h
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FCNTL_H
#define _SYS_FCNTL_H 1

#include <unistd.h>
#include <kpi/file.h>
#include <serena/types.h>
#include <serena/stat.h>

__CPP_BEGIN

// Same as open() with O_CREAT|O_WRONLY|O_TRUNC.
// @Concurrency: Safe
extern int creat(const char* _Nonnull path, mode_t mode);

// Opens an already existing file located at the filesystem location 'path'.
// Returns an error if the file does not exist or the caller lacks the necessary
// permissions to successfully open the file. 'mode' specifies whether the file
// should be opened for reading and/or writing. 'O_APPEND' may be passed in
// addition to 'O_WRONLY' to force the system to always append any newly written
// data to the file. The file position is disregarded by the write function(s) in
// this case.
// @Concurrency: Safe
extern int open(const char* _Nonnull path, int oflags, ...);

// Performs an operation on the given descriptor. The operation is performed on
// the descriptor itself rather than the underlying resource.
// @Concurrency: Safe
extern int fcntl(int fd, int cmd, ...);

__CPP_END

#endif /* _SYS_FCNTL_H */
