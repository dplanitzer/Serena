//
//  HIDChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "HIDChannel.h"
#include "HIDManager.h"


errno_t HIDChannel_Create(DriverRef _Nonnull pDriver, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    return DriverChannel_Create(class(HIDChannel), 0, kIOChannelType_Driver, mode, pDriver, pOutSelf);
}

errno_t HIDChannel_GetNextEvent(IOChannelRef _Nonnull self, struct timespec timeout, HIDEvent* _Nonnull evt)
{
    return HIDManager_GetNextEvent(gHIDManager, timeout, evt);
}

class_func_defs(HIDChannel, DriverChannel,
);
