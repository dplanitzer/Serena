//
//  fd.h
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FD_H
#define _SYS_FD_H 1

#include <kpi/_seek.h>
#include <kpi/_time.h>
#include <serena/types.h>

__CPP_BEGIN

// Standard I/O channels that are open when a process starts. These channels
// connect to the terminal input and output streams.
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2


// Closes the given I/O channel. All still pending data is written to the
// underlying device and then all resources allocated to the I/O channel are
// freed. If this function encounters an error while flushing pending data to
// the underlying device, then this error is recorded and returned by this
// function. However, note that the error does not stop this function from
// closing the channel. The I/O channel is guaranteed to be closed once this
// function returns. The error returned here is in this sense purely advisory.
// @Concurrency: Safe
extern int close(int fd);

// Returns 1 if the I/O channel is connected to a terminal and 0 otherwise.
extern int isatty(int fd);


// Sets the current file position. Note that the file position may be set to a
// value past the current file size. Doing this implicitly expands the size of
// the file to encompass the new file position. The byte range between the old
// end of file and the new end of file is automatically filled with zero bytes.
// @Concurrency: Safe
extern off_t lseek(int fd, off_t offset, int whence);


// Reads up to 'nbytes' bytes from the I/O channel 'fd' and writes them to the
// buffer 'buf'. The buffer must be big enough to hold 'nbytes' bytes. If at
// least one byte could be read successfully then the actual number of bytes
// read is returned as a positive number. If no bytes are available for reading
// because EOF is encountered then 0 is returned instead. If an error is
// encountered before at least one byte could be successfully read then -1 is
// returned and errno is set to a suitable error code. If however at least one
// byte could be successfully read before an error is encountered then all the
// successfully read bytes are returned.
// @Concurrency: Safe
extern ssize_t read(int fd, void* _Nonnull buf, size_t nbytes);

// Writes up to 'nbytes' bytes to the I/O channel 'fd'. The bytes are taken from
// the buffer 'buf' which must be big enough to hold 'nbytes' bytes. The number
// of bytes actually written is returned as a positive number. -1 is returned if
// an error is encountered before at least one byte could be successfully written
// to the destination. The errno variable is set to a suitable error in this
// case.
// @Concurrency: Safe
extern ssize_t write(int fd, const void* _Nonnull buf, size_t nbytes);


// Similar to truncate() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern int ftruncate(int fd, off_t length);

__CPP_END

#endif /* _SYS_FD_H */
