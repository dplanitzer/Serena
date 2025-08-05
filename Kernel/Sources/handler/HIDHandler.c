//
//  HIDHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "HIDHandler.h"
#include "HandlerChannel.h"
#include <driver/hid/HIDManager.h>
#include <kpi/fcntl.h>
#include <kpi/hid.h>
#include <kern/timespec.h>


final_class_ivars(HIDHandler, Handler,
);


errno_t HIDHandler_Create(HandlerRef _Nullable * _Nonnull pOutSelf)
{
    return Object_Create(class(HIDHandler), 0, (void**)pOutSelf);
}

errno_t HIDHandler_open(HandlerRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return HandlerChannel_Create(self, 0, SEO_FT_DRIVER, mode, 0, pOutChannel);
}

// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
errno_t HIDHandler_read(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
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

errno_t HIDHandler_ioctl(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, int cmd, va_list ap)
{
    switch (cmd) {
        case kHIDCommand_GetNextEvent: {
            const struct timespec* timeoutp = va_arg(ap, struct timespec*);
            HIDEvent* evt = va_arg(ap, HIDEvent*);

            return HIDManager_GetNextEvent(gHIDManager, timeoutp, evt);
        }

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
            return super_n(ioctl, Handler, HIDHandler, self, ioc, cmd, ap);
    }
}


class_func_defs(HIDHandler, Handler,
override_func_def(open, HIDHandler, Handler)
override_func_def(read, HIDHandler, Handler)
override_func_def(ioctl, HIDHandler, Handler)
);
