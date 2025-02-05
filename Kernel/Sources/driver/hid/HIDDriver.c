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
#include <System/HID.h>

final_class_ivars(HIDDriver, Driver,
);


errno_t HIDDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(HIDDriver, 0, NULL, pOutSelf);
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

errno_t HIDDriver_ioctl(DriverRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kHIDCommand_GetKeyRepeatDelays:
            HIDManager_GetKeyRepeatDelays(gHIDManager, va_arg(ap, TimeInterval*), va_arg(ap, TimeInterval*));
            return EOK;

        case kHIDCommand_SetKeyRepeatDelays:
            HIDManager_SetKeyRepeatDelays(gHIDManager, va_arg(ap, TimeInterval), va_arg(ap, TimeInterval));
            return EOK;

        case kHIDCommand_SetMouseCursor:
            return HIDManager_SetMouseCursor(gHIDManager, va_arg(ap, const uint16_t**), va_arg(ap, int), va_arg(ap, int), va_arg(ap, PixelFormat), va_arg(ap, int), va_arg(ap, int));

        case kHIDCommand_SetMouseCursorVisibility:
            return HIDManager_SetMouseCursorVisibility(gHIDManager, va_arg(ap, MouseCursorVisibility));

        case kHIDCommand_GetMouseCursorVisibility:
            return HIDManager_GetMouseCursorVisibility(gHIDManager);

        case kHIDCommand_ShieldMouseCursor:
            return HIDManager_ShieldMouseCursor(gHIDManager, va_arg(ap, int), va_arg(ap, int), va_arg(ap, int), va_arg(ap, int));

        case kHIDCommand_UnshieldMouseCursor:
            HIDManager_UnshieldMouseCursor(gHIDManager);
            return EOK;

        case kHIDCommand_GetPortDevice:
            return HIDManager_GetPortDevice(gHIDManager, va_arg(ap, int), va_arg(ap, InputType*));

        case kHIDCommand_SetPortDevice:
            return HIDManager_SetPortDevice(gHIDManager, va_arg(ap, int), va_arg(ap, InputType));

        default:
            return super_n(ioctl, Driver, HIDDriver, self, cmd, ap);
    }
}


class_func_defs(HIDDriver, Driver,
override_func_def(onStart, HIDDriver, Driver)
override_func_def(createChannel, HIDDriver, Driver)
override_func_def(read, HIDDriver, Driver)
override_func_def(ioctl, HIDDriver, Driver)
);
