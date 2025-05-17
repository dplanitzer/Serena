//
//  dirent.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _DIRENT_H
#define _DIRENT_H 1

#include <_cmndef.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <kpi/dirent.h>

__CPP_BEGIN

struct _DIR;
typedef struct _DIR DIR;

#define __DIR_BASE   32


// Opens the directory at the filesystem location 'path' for reading. Call this
// function to obtain an I/O channel suitable for reading the content of the
// directory. Call IOChannel_Close() once you are done with the directory.
// @Concurrency: Safe
extern DIR* _Nullable opendir(const char* _Nonnull path);

// Closes the given directory descriptor.
// @Concurrency: Safe
extern int closedir(DIR* _Nullable dir);

// Reads one or more directory entries from the directory identified by 'ioc'.
// Returns the number of bytes actually read and returns 0 once all directory
// entries have been read.
// You can rewind to the beginning of the directory listing by calling
// rewinddir().
// @Concurrency: Safe
extern errno_t readdir(DIR* _Nonnull dir, dirent_t* _Nonnull entries, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead);

// Resets the read position of the directory identified by 'ioc' to the beginning.
// The next readdir() call will start reading directory entries from the
// beginning of the directory.
// @Concurrency: Safe
extern errno_t rewinddir(DIR* _Nonnull dir);

__CPP_END

#endif /* _DIRENT_H */
