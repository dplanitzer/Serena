//
//  ConsoleChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsoleChannel.h"
#include <kpi/fcntl.h>


errno_t ConsoleChannel_Create(ConsoleRef _Nonnull pConsole, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    return DriverChannel_Create(class(ConsoleChannel), 0, SEO_FT_TERMINAL, mode, (DriverRef)pConsole, pOutSelf);
}


class_func_defs(ConsoleChannel, DriverChannel,
);
