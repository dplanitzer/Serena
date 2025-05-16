//
//  dirent.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _DIRENT_H
#define _DIRENT_H 1

#include <kern/_cmndef.h>
#include <kern/_syslimits.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/_dirent.h>

__CPP_BEGIN

// Opens the directory at the filesystem location 'path' for reading. Call this
// function to obtain an I/O channel suitable for reading the content of the
// directory. Call IOChannel_Close() once you are done with the directory.
// @Concurrency: Safe
extern errno_t opendir(const char* _Nonnull path, int* _Nonnull fd);

// Reads one or more directory entries from the directory identified by 'ioc'.
// Returns the number of bytes actually read and returns 0 once all directory
// entries have been read.
// You can rewind to the beginning of the directory listing by calling
// rewinddir().
// @Concurrency: Safe
extern errno_t readdir(int fd, dirent_t* _Nonnull entries, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead);

// Resets the read position of the directory identified by 'ioc' to the beginning.
// The next readdir() call will start reading directory entries from the
// beginning of the directory.
// @Concurrency: Safe
extern errno_t rewinddir(int fd);

__CPP_END

#endif /* _DIRENT_H */
