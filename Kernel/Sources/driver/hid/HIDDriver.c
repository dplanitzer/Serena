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
    DriverEntry de;
    de.name = "hid";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = FilePermissions_MakeFromOctal(0666);
    de.arg = 0;

    return Driver_Publish(self, &de);
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
    const bool isNonBlocking = (IOChannel_GetMode(pChannel) & O_NONBLOCK) == O_NONBLOCK;
    const TimeInterval timeout = (isNonBlocking) ? kTimeInterval_Zero : kTimeInterval_Infinity;
    HIDEvent* pe = buf;
    ssize_t nBytesRead = 0;

    while ((nBytesRead + sizeof(HIDEvent)) <= nBytesToRead) {
        // Only block waiting for the first event. For all other events we do not
        // wait.
        const errno_t e1 = HIDManager_GetNextEvent(gHIDManager, (pe == buf) ? timeout : kTimeInterval_Zero, pe);

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

errno_t HIDDriver_ioctl(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kHIDCommand_GetNextEvent: {
            const TimeInterval timeout = va_arg(ap, TimeInterval);
            HIDEvent* evt = va_arg(ap, HIDEvent*);

            return HIDManager_GetNextEvent(gHIDManager, timeout, evt);
        }

        case kHIDCommand_GetKeyRepeatDelays: {
            TimeInterval* initial = va_arg(ap, TimeInterval*);
            TimeInterval* repeat = va_arg(ap, TimeInterval*);

            HIDManager_GetKeyRepeatDelays(gHIDManager, initial, repeat);
            return EOK;
        }

        case kHIDCommand_SetKeyRepeatDelays: {
            const TimeInterval initial = va_arg(ap, TimeInterval);
            const TimeInterval repeat = va_arg(ap, TimeInterval);

            HIDManager_SetKeyRepeatDelays(gHIDManager, initial, repeat);
            return EOK;
        }

        case kHIDCommand_SetMouseCursor: {
            const uint16_t** planes = va_arg(ap, const uint16_t**);
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const PixelFormat fmt = va_arg(ap, PixelFormat);
            const int hotSpotX = va_arg(ap, int);
            const int hotSpotY = va_arg(ap, int);

            return HIDManager_SetMouseCursor(gHIDManager, planes, width, height, fmt, hotSpotX, hotSpotY);
        }

        case kHIDCommand_SetMouseCursorVisibility: {
            const MouseCursorVisibility vis = va_arg(ap, MouseCursorVisibility);

            return HIDManager_SetMouseCursorVisibility(gHIDManager, vis);
        }

        case kHIDCommand_GetMouseCursorVisibility:
            return HIDManager_GetMouseCursorVisibility(gHIDManager);

        case kHIDCommand_ShieldMouseCursor: {
            const int x = va_arg(ap, int);
            const int y = va_arg(ap, int);
            const int w = va_arg(ap, int);
            const int h = va_arg(ap, int);

            return HIDManager_ShieldMouseCursor(gHIDManager, x, y, w, h);
        }

        case kHIDCommand_UnshieldMouseCursor:
            HIDManager_UnshieldMouseCursor(gHIDManager);
            return EOK;

        case kHIDCommand_GetPortDevice: {
            const int port = va_arg(ap, int);
            InputType* itype = va_arg(ap, InputType*);

            return HIDManager_GetPortDevice(gHIDManager, port, itype);
        }

        case kHIDCommand_SetPortDevice: {
            const int port = va_arg(ap, int);
            const InputType itype = va_arg(ap, InputType);

            return HIDManager_SetPortDevice(gHIDManager, port, itype);
        }

        default:
            return super_n(ioctl, Driver, HIDDriver, self, pChannel, cmd, ap);
    }
}


class_func_defs(HIDDriver, Driver,
override_func_def(onStart, HIDDriver, Driver)
override_func_def(createChannel, HIDDriver, Driver)
override_func_def(read, HIDDriver, Driver)
override_func_def(ioctl, HIDDriver, Driver)
);
