//
//  IOChannel.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_IOCHANNEL_H
#define _SYS_IOCHANNEL_H 1

#include <System/_cmndef.h>
#include <System/Error.h>
#include <System/Types.h>

__CPP_BEGIN

#define IOResourceCommand(__cmd) (__cmd)
#define IOChannelCommand(__cmd) -(__cmd)
#define IsIOChannelCommand(__cmd) ((__cmd) < 0)

// Returns the type of an I/O channel. The type indicates to which kind of
// I/O resource the channel is connected and thus which kind of operations are
// supported by the channel.
#define kIOChannelCommand_GetType   IOChannelCommand(1)
typedef enum IOChannelType {
    kIOChannelType_Terminal,
    kIOChannelType_File,
    kIOChannelType_Directory,
    kIOChannelType_Pipe,
    kIOChannelType_Driver,
    kIOChannelType_Filesystem
} IOChannelType;

// Returns the mode with which the I/O channel was opened.
// unsigned int get_mode(int ioc)
#define kIOChannelCommand_GetMode   IOChannelCommand(2)

// Updates the mode of an I/O channel. Enables 'mode' on the channel if 'setOrClear'
// is != 0 and disables 'mode' if 'setOrClear' == 0.
// The following modes may be changed:
// - kOpen_Append
// - kOpen_NonBlocking
// errno_t set_mode(int ioc, int setOrClear, unsigned int mode)
#define kIOChannelCommand_SetMode   IOChannelCommand(3)


// Standard I/O channels that are open when a process starts. These channels
// connect to the terminal input and output streams.
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2


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
extern errno_t read(int ioc, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

// Writes up to 'nBytesToWrite' bytes to the I/O channel 'ioc'. The bytes are
// taken from the buffer 'buffer' which must be big enough to hold 'nBytesToWrite'
// bytes. The number of bytes actually written is returned in 'nOutBytesWritten'.
// This function returns EOK and the number of successfully written bytes if it
// is able to write at least one byte successfully before it encounters an error.
// It however returns a suitable error code and 0 in 'nOutBytesWritten' if it
// encounters an error before it is able to write at least one byte. 
// @Concurrency: Safe
extern errno_t write(int ioc, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);


// Closes the given I/O channel. All still pending data is written to the
// underlying device and then all resources allocated to the I/O channel are
// freed. If this function encounters an error while flushing pending data to
// the underlying device, then this error is recorded and returned by this
// function. However, note that the error does not stop this function from
// closing the channel. The I/O channel is guaranteed to be closed once this
// function returns. The error returned here is in this sense purely advisory.
// @Concurrency: Safe
extern errno_t close(int ioc);


// Returns the type of the I/O channel. See the IOChannelType enumeration.
// @Concurrency: Safe
extern IOChannelType fgettype(int ioc);

// Returns the mode with which the I/O channel was originally opened. The exact
// meaning of mode depends on the I/O channel type. Ie see open() for the
// file specific modes.
// @Concurrency: Safe
extern unsigned int fgetmode(int ioc);


// Invokes a I/O channel specific method on the I/O channel 'ioc'.
// @Concurrency: Safe
extern errno_t fiocall(int ioc, int cmd, ...);

__CPP_END

#endif /* _SYS_IOCHANNEL_H */
