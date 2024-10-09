//
//  ConsoleChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsoleChannel.h"


errno_t ConsoleChannel_Create(ConsoleRef _Nonnull pConsole, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    return DriverChannel_Create(&kConsoleChannelClass, (DriverRef)pConsole, mode, pOutSelf);
}

errno_t ConsoleChannel_ioctl(ConsoleChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetType:
            *((int*) va_arg(ap, int*)) = kIOChannelType_Terminal;
            return EOK;

        default:
            return super_n(ioctl, IOChannel, ConsoleChannel, self, cmd, ap);
    }
}


class_func_defs(ConsoleChannel, DriverChannel,
override_func_def(ioctl, ConsoleChannel, IOChannel)
);
