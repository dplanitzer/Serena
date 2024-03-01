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

// Returns the type of the channel
// IOChannel_Control(int fd, int cmd, int _Nonnull *pOutType)
#define kIOChannelCommand_GetType   IOChannelCommand(1)
typedef enum {
    kIOChannelType_Terminal,
    kIOChannelType_File,
    kIOChannelType_Directory
} IOChannelType;

#define kIOChannelCommand_GetMode   IOChannelCommand(2)


#if !defined(__KERNEL__)

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
extern errno_t IOChannel_Read(int ioc, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

// Writes up to 'nBytesToWrite' bytes to the I/O channel 'ioc'. The bytes are
// taken from the buffer 'buffer' which must be big enough to hold 'nBytesToWrite'
// bytes. The number of bytes actually written is returned in 'nOutBytesWritten'.
// This function returns EOK and the number of successfully written bytes if it
// is able to write at least one byte successfully before it encounters an error.
// It however returns a suitable error code and 0 in 'nOutBytesWritten' if it
// encounters an error before it is able to write at least one byte. 
// @Concurrency: Safe
extern errno_t IOChannel_Write(int ioc, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);


// Closes the given I/O channel. All still pending data is written to the
// underlying device and then all resources allocated to the I/O channel are
// freed. If this function encounters an error while flushing pending data to
// the underlying device, then this error is recorded and returned by this
// function. However, note that the error does not stop this function from
// closing the channel. The I/O channel is guaranteed to be closed once this
// function returns. The error returned here is in this sense purely advisory.
// @Concurrency: Safe
extern errno_t IOChannel_Close(int ioc);


// Returns the type of the I/O channel. See the IOChannelType enumeration.
// @Concurrency: Safe
extern IOChannelType IOChannel_GetType(int ioc);

// Returns the mode with which the I/O channel was originally opened. The exact
// meaning of mode depends on the I/O channel type. Ie see File_Open for the
// file specific modes.
// @Concurrency: Safe
extern unsigned int IOChannel_GetMode(int ioc);


// Invokes a I/O channel specific method on the I/O channel 'ioc'.
// @Concurrency: Safe
extern errno_t IOChannel_Control(int ioc, int cmd, ...);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_IOCHANNEL_H */
