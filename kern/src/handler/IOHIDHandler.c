//
//  IOHIDHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IOHIDHandler.h"
#include <driver/hid/HIDDriver.h>


errno_t IOHIDHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return IODriverHandler_Create(class(IOHIDHandler), FD_TYPE_DRIVER, ip, flags, pOutHandler);
}

// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
errno_t IOHIDHandler_read(struct IOHIDHandler* _Nonnull self, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    HIDDriverRef drv = IODriverHandler_GetDriver(self);
    const fd_flags_t flags = Handler_GetFlags(self);
    const bool isNonBlocking = (flags & O_NONBLOCK) == O_NONBLOCK;
    const nanotime_t* timp = (isNonBlocking) ? &NANOTIME_ZERO : &NANOTIME_INF;
    HIDEvent* pe = buf;
    ssize_t nBytesRead = 0;

    if ((flags & O_RDONLY) == 0) {
        return EBADF;
    }


    while ((nBytesRead + sizeof(HIDEvent)) <= nBytesToRead) {
        // Only block waiting for the first event. For all other events we do not
        // wait.
        const errno_t e1 = HIDDriver_GetNextEvent(drv, (pe == buf) ? timp : &NANOTIME_ZERO, pe);

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

errno_t IOHIDHandler_control(struct IOHIDHandler* _Nonnull self, int cmd, va_list ap)
{
    HIDDriverRef drv = IODriverHandler_GetDriver(self);

    switch (cmd) {
        case kHIDCommand_GetNextEvent: {
            const nanotime_t* timeoutp = va_arg(ap, nanotime_t*);
            HIDEvent* evt = va_arg(ap, HIDEvent*);

            return HIDDriver_GetNextEvent(drv, timeoutp, evt);
        }

        case kHIDCommand_FlushEvents:
            HIDDriver_FlushEvents(drv);
            return EOK;
            
        case kHIDCommand_GetKeyRepeatDelays: {
            nanotime_t* initialp = va_arg(ap, nanotime_t*);
            nanotime_t* repeatp = va_arg(ap, nanotime_t*);

            HIDDriver_GetKeyRepeatDelays(drv, initialp, repeatp);
            return EOK;
        }

        case kHIDCommand_SetKeyRepeatDelays: {
            const nanotime_t* initialp = va_arg(ap, nanotime_t*);
            const nanotime_t* repeatp = va_arg(ap, nanotime_t*);

            HIDDriver_SetKeyRepeatDelays(drv, initialp, repeatp);
            return EOK;
        }

        case kHIDCommand_ObtainCursor: {
            return HIDDriver_ObtainCursor(drv);
        }

        case kHIDCommand_ReleaseCursor:
            HIDDriver_ReleaseCursor(drv);
            return EOK;

        case kHIDCommand_SetCursor: {
            const void** planes = va_arg(ap, const void**);
            const size_t bytesPerRow = va_arg(ap, size_t);
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const pixfmt_t format = va_arg(ap, pixfmt_t);
            const int hotSpotX = va_arg(ap, int);
            const int hotSpotY = va_arg(ap, int);

            return HIDDriver_SetCursor(drv, planes, bytesPerRow, width, height, format, hotSpotX, hotSpotY);
        }

        case kHIDCommand_ShowCursor:
            HIDDriver_ShowCursor(drv);
            return EOK;

        case kHIDCommand_HideCursor:
            HIDDriver_HideCursor(drv);
            return EOK;

        case kHIDCommand_ObscureCursor:
            HIDDriver_ObscureCursor(drv);
            return EOK;

        case kHIDCommand_ShieldCursor: {
            const int x = va_arg(ap, int);
            const int y = va_arg(ap, int);
            const int w = va_arg(ap, int);
            const int h = va_arg(ap, int);

            return HIDDriver_ShieldMouseCursor(drv, x, y, w, h);
        }

        default:
            return Handler_Super_Control(IOHIDHandler, cmd, ap);
    }
}


class_func_defs(IOHIDHandler, IODriverHandler,
override_func_def(read, IOHIDHandler, Handler)
override_func_def(control, IOHIDHandler, Handler)
);
