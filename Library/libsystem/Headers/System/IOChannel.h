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
#include <System/_nulldef.h>
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

extern errno_t IOChannel_Read(int ioc, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);
extern errno_t IOChannel_Write(int ioc, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

extern errno_t IOChannel_Close(int ioc);


extern IOChannelType IOChannel_GetType(int ioc);
extern int IOChannel_GetMode(int ioc);


extern errno_t IOChannel_Control(int ioc, int cmd, ...);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_IOCHANNEL_H */
