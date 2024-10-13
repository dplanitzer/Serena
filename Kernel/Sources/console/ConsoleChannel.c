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
    return DriverChannel_Create(&kConsoleChannelClass, kIOChannelType_Terminal, mode, (DriverRef)pConsole, pOutSelf);
}


class_func_defs(ConsoleChannel, DriverChannel,
);
