//
//  Directory.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DIRECTORY_H
#define _SYS_DIRECTORY_H 1

#include <System/_cmndef.h>
#include <System/_syslimits.h>
#include <System/Error.h>
#include <System/FilePermissions.h>
#include <sys/types.h>

__CPP_BEGIN

typedef struct dirent {
    ino_t       inid;
    char        name[__PATH_COMPONENT_MAX];
} dirent_t;


// Opens the directory at the filesystem location 'path' for reading. Call this
// function to obtain an I/O channel suitable for reading the content of the
// directory. Call IOChannel_Close() once you are done with the directory.
// @Concurrency: Safe
extern errno_t opendir(const char* _Nonnull path, int* _Nonnull ioc);

// Reads one or more directory entries from the directory identified by 'ioc'.
// Returns the number of bytes actually read and returns 0 once all directory
// entries have been read.
// You can rewind to the beginning of the directory listing by calling
// rewinddir().
// @Concurrency: Safe
extern errno_t readdir(int ioc, dirent_t* _Nonnull entries, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead);

// Resets the read position of the directory identified by 'ioc' to the beginning.
// The next readdir() call will start reading directory entries from the
// beginning of the directory.
// @Concurrency: Safe
extern errno_t rewinddir(int ioc);

__CPP_END

#endif /* _SYS_DIRECTORY_H */
