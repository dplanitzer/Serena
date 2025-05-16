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
#include <stdnoreturn.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/_unistd.h>

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
extern int getcwd(char* _Nonnull buffer, size_t bufferSize);

extern int chown(const char* _Nonnull path, uid_t uid, gid_t gid);

// Closes the given I/O channel. All still pending data is written to the
// underlying device and then all resources allocated to the I/O channel are
// freed. If this function encounters an error while flushing pending data to
// the underlying device, then this error is recorded and returned by this
// function. However, note that the error does not stop this function from
// closing the channel. The I/O channel is guaranteed to be closed once this
// function returns. The error returned here is in this sense purely advisory.
// @Concurrency: Safe
extern int close(int fd);

extern _Noreturn _exit(int status);

extern uid_t getuid(void);
extern gid_t getgid(void);

extern pid_t getpid(void);
extern pid_t getppid(void);


// Returns 1 if the I/O channel is connected to a terminal and 0 otherwise.
extern int isatty(int fd);


// Sets the current file position. Note that the file position may be set to a
// value past the current file size. Doing this implicitly expands the size of
// the file to encompass the new file position. The byte range between the old
// end of file and the new end of file is automatically filled with zero bytes.
// @Concurrency: Safe
extern off_t lseek(int fd, off_t offset, int whence);


#define PIPE_RD 0
#define PIPE_WR 1

// Creates an anonymous pipe and returns a read and write I/O channel to the pipe.
// Data which is written to the pipe using the write I/O channel can be read using
// the read I/O channel. The data is made available in first-in-first-out order.
// Note that both I/O channels must be closed to free all pipe resources.
// The write channel is returned in fds[1] and the read channel in fds[0].
extern int pipe(int fds[2]);


// Reads up to 'nBytesToRead' bytes from the I/O channel 'ioc' and writes them
// to the buffer 'buffer'. The buffer must be big enough to hold 'nBytesToRead'
// bytes. The number of bytes actually read is returned in 'nOutBytesRead'.
// If at least one byte could be read successfully then  'nOutBytesRead' is set
// to the number of bytes read. If no bytes are available for reading because EOF
// is encountered then 'nOutBytesRead' is set to 0 and EOK is returned as the
// function status. If an error is encountered before at least one byte could be
// successfully read then this function returns 0 in 'nOutBytesRead' and a
// suitable error code. If however at least one byte could be successfully read
// before an error is encountered then the successfully read bytes and EOK is
// returned.
// @Concurrency: Safe
extern errno_t read(int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

// Writes up to 'nBytesToWrite' bytes to the I/O channel 'ioc'. The bytes are
// taken from the buffer 'buffer' which must be big enough to hold 'nBytesToWrite'
// bytes. The number of bytes actually written is returned in 'nOutBytesWritten'.
// This function returns EOK and the number of successfully written bytes if it
// is able to write at least one byte successfully before it encounters an error.
// It however returns a suitable error code and 0 in 'nOutBytesWritten' if it
// encounters an error before it is able to write at least one byte. 
// @Concurrency: Safe
extern errno_t write(int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);


// Truncates the file at the filesystem location 'path'. If the new length is
// greater than the size of the existing file, then the file is expanded and the
// newly added data range is zero-filled. If the new length is less than the
// size of the existing file, then the excess data is removed and the size of
// the file is set to the new length.
// @Concurrency: Safe
extern int truncate(const char* _Nonnull path, off_t length);

// Similar to truncate() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern int ftruncate(int fd, off_t length);

// Deletes the file or (empty) directory located at the filesystem location 'path'.
// Note that this function deletes empty directories only.
// @Concurrency: Safe
extern int unlink(const char* _Nonnull path);


// Synchronously writes all dirty disk blocks back to disk.
extern void sync(void);

__CPP_END

#endif /* _UNISTD_H */
