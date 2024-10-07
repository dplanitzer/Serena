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
    
    if ((err = DriverChannel_Create(&kEventChannelClass, (DriverRef)pDriver, mode, (IOChannelRef*)&self)) == EOK) {
        self->timeout = kTimeInterval_Infinity;
    }

    *pOutSelf = (IOChannelRef)self;
    return err;
}

errno_t EventChannel_copy(EventChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    EventChannelRef pNewChannel;

    if ((err = super_n(copy, IOChannel, self, (IOChannelRef*)&pNewChannel)) == EOK) {
        pNewChannel->timeout = self->timeout;
    }

    *pOutChannel = (IOChannelRef)pNewChannel;
    return err;
}


class_func_defs(EventChannel, DriverChannel,
override_func_def(copy, EventChannel, IOChannel)
);
