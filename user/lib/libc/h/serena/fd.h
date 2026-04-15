//
//  fd.h
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_FD_H
#define _SERENA_FD_H 1

#include <kpi/fd.h>
#include <kpi/_seek.h>
#include <kpi/_time.h>
#include <kpi/attr.h>
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
extern int fd_close(int fd);


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
extern ssize_t fd_read(int fd, void* _Nonnull buf, size_t nbytes);

// Writes up to 'nbytes' bytes to the I/O channel 'fd'. The bytes are taken from
// the buffer 'buf' which must be big enough to hold 'nbytes' bytes. The number
// of bytes actually written is returned as a positive number. -1 is returned if
// an error is encountered before at least one byte could be successfully written
// to the destination. The errno variable is set to a suitable error in this
// case.
// @Concurrency: Safe
extern ssize_t fd_write(int fd, const void* _Nonnull buf, size_t nbytes);


// Sets the current file position. Note that the file position may be set to a
// value past the current file size. Doing this implicitly expands the size of
// the file to encompass the new file position. The byte range between the old
// end of file and the new end of file is automatically filled with zero bytes.
// @Concurrency: Safe
extern off_t fd_seek(int fd, off_t offset, int whence);


// Similar to fs_truncate() but operates on the open file identified by 'fd'.
// @Concurrency: Safe
extern int fd_truncate(int fd, off_t length);


// Returns a copy of file/directory attributes. Similar to fs_attr() but
// operates on the file descriptor 'fd'.
// @Concurrency: Safe
extern int fd_attr(int fd, fs_attr_t* _Nonnull attr);


// Returns information about the descriptor 'fd'. The kind of information that
// should be returned if specified by 'flavor'. 'info' must point to a memory
// block that is big enough to hold the corresponding info data structure.
// @Concurrency: Safe
extern int fd_info(int fd, int flavor, fd_info_ref _Nonnull info);

// Convenience function that returns the type of the descriptor. FD_TYPE_INVALID
// is returned if the descriptor is not valid.
// @Concurrency: Safe
extern int fd_type(int fd);


// Updates the descriptor flags by combining the current descriptor flags with
// the new flags 'flags' based o the combination operation 'op'.
// @Concurrency: Safe
extern int fd_setflags(int fd, int op, int flags);


// Creates a new reference to the descriptor 'fd' and assigns it to a new
// descriptor which it returns. The newly allocated descriptor id is greater or
// equal 'min_fd'. Returns -1 and sets errno accordingly on an error.
// @Concurrency: Safe
extern int fd_dup(int fd, int min_fd);

// Creates a new reference to the existing descriptor 'fd' and assigns it to the
// descriptor 'target_fd'. 'target_fd' is closed if it is a valid descriptor. An
// unused is selected if 'target_fd' is < 0.
// Returns -1 and sets errno appropriately on an error.
extern int fd_dup_to(int fd, int target_fd);

__CPP_END

#endif /* _SERENA_FD_H */
