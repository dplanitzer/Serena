//
//  HIDDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "HIDDriver.h"
#include <driver/hid/HIDManager.h>
#include <filesystem/IOChannel.h>
#include <kpi/fcntl.h>
#include <kpi/hid.h>
#include <kern/timespec.h>


final_class_ivars(HIDDriver, PseudoDriver,
);


errno_t HIDDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return PseudoDriver_Create(class(HIDDriver), 0, pOutSelf);
}

errno_t HIDDriver_onStart(HIDDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    DriverEntry de;

    de.dirId = Driver_GetBusDirectory((DriverRef)self);
    de.name = "hid";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.driver = (HandlerRef)self;
    de.arg = 0;

    return Driver_Publish(self, &de);
}

// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
errno_t HIDDriver_read(HIDDriverRef _Nonnull self, IOChannelRef _Nonnull ioc, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    const bool isNonBlocking = (IOChannel_GetMode(ioc) & O_NONBLOCK) == O_NONBLOCK;
    const struct timespec* timp = (isNonBlocking) ? &TIMESPEC_ZERO : &TIMESPEC_INF;
    HIDEvent* pe = buf;
    ssize_t nBytesRead = 0;

    while ((nBytesRead + sizeof(HIDEvent)) <= nBytesToRead) {
        // Only block waiting for the first event. For all other events we do not
        // wait.
        const errno_t e1 = HIDManager_GetNextEvent(gHIDManager, (pe == buf) ? timp : &TIMESPEC_ZERO, pe);

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

errno_t HIDDriver_ioctl(HIDDriverRef _Nonnull self, IOChannelRef _Nonnull ioc, int cmd, va_list ap)
{
    switch (cmd) {
        case kHIDCommand_GetNextEvent: {
            const struct timespec* timeoutp = va_arg(ap, struct timespec*);
            HIDEvent* evt = va_arg(ap, HIDEvent*);

            return HIDManager_GetNextEvent(gHIDManager, timeoutp, evt);
        }

        case kHIDCommand_FlushEvents:
            HIDManager_FlushEvents(gHIDManager);
            return EOK;
            
        case kHIDCommand_GetKeyRepeatDelays: {
            struct timespec* initialp = va_arg(ap, struct timespec*);
            struct timespec* repeatp = va_arg(ap, struct timespec*);

            HIDManager_GetKeyRepeatDelays(gHIDManager, initialp, repeatp);
            return EOK;
        }

        case kHIDCommand_SetKeyRepeatDelays: {
            const struct timespec* initialp = va_arg(ap, struct timespec*);
            const struct timespec* repeatp = va_arg(ap, struct timespec*);

            HIDManager_SetKeyRepeatDelays(gHIDManager, initialp, repeatp);
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

        default:
            return super_n(ioctl, Handler, HIDDriver, self, ioc, cmd, ap);
    }
}


class_func_defs(HIDDriver, PseudoDriver,
override_func_def(onStart, HIDDriver, Driver)
override_func_def(read, HIDDriver, Handler)
override_func_def(ioctl, HIDDriver, Handler)
);
