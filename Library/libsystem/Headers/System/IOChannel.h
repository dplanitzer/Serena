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
    kIOChannelType_Filesystem,
    kIOChannelType_Process,
} IOChannelType;

// Returns the mode with which the I/O channel was opened.
// unsigned int get_mode(int ioc)
#define kIOChannelCommand_GetMode   IOChannelCommand(2)

// Updates the mode of an I/O channel. Enables 'mode' on the channel if 'setOrClear'
// is != 0 and disables 'mode' if 'setOrClear' == 0.
// The following modes may be changed:
// - O_APPEND
// - O_NONBLOCK
// errno_t set_mode(int ioc, int setOrClear, unsigned int mode)
#define kIOChannelCommand_SetMode   IOChannelCommand(3)





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
