//
//  HIDDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "HIDDriver.h"
#include "HIDChannel.h"
#include "HIDManager.h"

final_class_ivars(HIDDriver, Driver,
);


errno_t HIDDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(HIDDriver, 0, pOutSelf);
}

errno_t HIDDriver_onStart(DriverRef _Nonnull _Locked self)
{
    return Driver_Publish(self, "hid", 0);
}

errno_t HIDDriver_createChannel(DriverRef _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return HIDChannel_Create(self, mode, pOutChannel);
}

// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
errno_t HIDDriver_read(DriverRef _Nonnull self, HIDChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return HIDManager_GetEvents(gHIDManager, pBuffer, nBytesToRead, pChannel->timeout, nOutBytesRead);
}


class_func_defs(HIDDriver, Driver,
override_func_def(onStart, HIDDriver, Driver)
override_func_def(createChannel, HIDDriver, Driver)
override_func_def(read, HIDDriver, Driver)
);
