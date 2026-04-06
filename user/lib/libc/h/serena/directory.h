//
//  directory.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_DIRECTORY_H
#define _SERENA_DIRECTORY_H 1

#include <_cmndef.h>
#include <kpi/directory.h>
#include <serena/types.h>

__CPP_BEGIN

// Directory API and concurrency:
// These calls are not internally protected by a mutex. Thus application code is
// responsible for providing its own locking if more than one thread of execution
// is going to call functions on the same dir_t object.
//
// Rational: adding locking to the calls would not be sufficient to make them
// concurrency safe. Eg an app creates a dir_t and then has 8 threads of execution
// reading the contents of the dir_t in parallel until EOF is encountered. The
// problem is that the first thread that encounters EOF can not call dir_close()
// because the 7 other threads may currently be busy doing something else (not
// calling dir_read() because they are processing data read previously). The guy
// who is calling dir_close() would execute the dir_close() atomically, however
// the dir_t pointer becomes stale after dir_close() returns. If now the other 7
// guys try to call dir_read(), then they will cause a crash because they are
// trying to use a stale dir_t pointer.
// The only way to fix this is by making sure that the application code itself
// introduces proper coordination between the 8 threads of execution. To do this
// it has to introduce a lock of its own that protects that dir_t. Thus we would
// end up with two nested locks per dir_t.
//
// Thus the dir_t calls do not provide built-in concurrency protection. However the
// underlying filesystem layer does with respect to other processes.


// Opens the directory at the filesystem location 'path' for reading. Call this
// function to obtain an I/O channel suitable for reading the content of the
// directory. Call IOChannel_Close() once you are done with the directory.
// @Concurrency: Not Safe
extern dir_t* _Nullable dir_open(const char* _Nonnull path);

// Closes the given directory descriptor.
// @Concurrency: Not Safe
extern int dir_close(dir_t* _Nullable dir);

// Reads the next directory entry and returns a pointer to it if it exists.
// Returns NULL if no more entry exists in the directory.
// You can rewind to the beginning of the directory listing by calling
// dir_rewind().
// @Concurrency: Not Safe
extern const dir_entry_t* _Nullable dir_read(dir_t* _Nonnull dir);

// Resets the read position of the directory identified by 'ioc' to the beginning.
// The next dir_read() call will start reading directory entries from the
// beginning of the directory.
// @Concurrency: Not Safe
extern void dir_rewind(dir_t* _Nonnull dir);


// Creates an empty directory with the name and at the filesystem location specified
// by 'path'. 'mode' specifies the permissions that should be assigned to the
// directory.
// @Concurrency: Safe
extern int mkdir(const char* _Nonnull path, mode_t mode);

// Deletes the empty directory located at the filesystem location 'path'.
// Note that this function deletes empty directories only.
// @Concurrency: Safe
extern int rmdir(const char* _Nonnull path);

__CPP_END

#endif /* _SERENA_DIRECTORY_H */
