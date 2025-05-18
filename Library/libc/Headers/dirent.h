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


// DIR  API and concurrency:
// These calls are not internally protected by a mutex. Thus application code is
// responsible for providing its own locking if more than one thread of execution
// is going to call functions on the same DIR object.
//
// Rational: adding locking to the DIR calls would not be sufficient to make them
// concurrency safe. Eg an app creates a DIR and then has 8 threads of execution
// reading the contents of the DIR in parallel until EOF is encountered. The
// problem is that the first thread that encounters EOF can not call closedir()
// because the 7 other threads may currently be busy doing something else (not
// calling readdir() because they are processing data read previously). The guy
// who is calling closedir() would execute the closedir() atomically, however
// the DIR pointer becomes stale after closedir() returns. If now the other 7
// guys try to call readdir(), then they will cause a crash because they are
// trying to use a stale DIR pointer.
// The only way to fix this is by making sure that the application code itself
// introduces proper coordination between the 8 threads of execution. To do this
// it has to introduce a lock of its own that protects that DIR. Thus we would
// end up with two nested locks per DIR.
//
// Thus the DIR calls do not provide built-in concurrency protection. However the
// underlying filesystem layer does with respect to other processes.


// Opens the directory at the filesystem location 'path' for reading. Call this
// function to obtain an I/O channel suitable for reading the content of the
// directory. Call IOChannel_Close() once you are done with the directory.
// @Concurrency: Not Safe
extern DIR* _Nullable opendir(const char* _Nonnull path);

// Closes the given directory descriptor.
// @Concurrency: Not Safe
extern int closedir(DIR* _Nullable dir);

// Reads one or more directory entries from the directory identified by 'ioc'.
// Returns the number of bytes actually read and returns 0 once all directory
// entries have been read.
// You can rewind to the beginning of the directory listing by calling
// rewinddir().
// @Concurrency: Not Safe
extern struct dirent* _Nullable readdir(DIR* _Nonnull dir);

// Resets the read position of the directory identified by 'ioc' to the beginning.
// The next readdir() call will start reading directory entries from the
// beginning of the directory.
// @Concurrency: Not Safe
extern void rewinddir(DIR* _Nonnull dir);

__CPP_END

#endif /* _DIRENT_H */
