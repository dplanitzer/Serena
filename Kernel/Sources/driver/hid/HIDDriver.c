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
    return Driver_Create(class(HIDDriver), 0, NULL, pOutSelf);
}

errno_t HIDDriver_onStart(DriverRef _Nonnull _Locked self)
{
    return Driver_Publish(self, "hid", kUserId_Root, kGroupId_Root, FilePermissions_MakeFromOctal(0666), 0);
}

errno_t HIDDriver_createChannel(DriverRef _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return HIDChannel_Create(self, mode, pOutChannel);
}

// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
errno_t HIDDriver_read(DriverRef _Nonnull self, HIDChannelRef _Nonnull pChannel, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    HIDEvent* pe = buf;
    ssize_t nBytesRead = 0;

    while ((nBytesRead + sizeof(HIDEvent)) <= nBytesToRead) {
        // Only block waiting for the first event. For all other events we do not
        // wait.
        const errno_t e1 = HIDManager_GetNextEvent(gHIDManager, (pe == buf) ? pChannel->timeout : kTimeInterval_Zero, pe);

        if (e1 != EOK) {
            // Return with an error if we were not able to read any event data at
            // all and return with the data we were able to read otherwise.
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        nBytesRead += sizeof(HIDEvent);
        pe++;
    }

    *nOutBytesRead = nBytesRead;
    return err;
}

errno_t HIDDriver_ioctl(DriverRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kHIDCommand_GetNextEvent:
            return HIDManager_GetNextEvent(gHIDManager, va_arg(ap, TimeInterval), va_arg(ap, HIDEvent*));

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
