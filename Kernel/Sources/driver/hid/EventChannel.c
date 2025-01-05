//
//  EventChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "EventChannel.h"
#include "EventDriver.h"


errno_t EventChannel_Create(EventDriverRef _Nonnull pDriver, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    EventChannelRef self = NULL;
    
    if ((err = DriverChannel_Create(&kEventChannelClass, 0, kIOChannelType_Driver, mode, (DriverRef)pDriver, (IOChannelRef*)&self)) == EOK) {
        self->timeout = kTimeInterval_Infinity;
    }

    *pOutSelf = (IOChannelRef)self;
    return err;
}


class_func_defs(EventChannel, DriverChannel,
);
