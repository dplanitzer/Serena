//
//  HIDDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "HIDDriver.h"
#include <driver/hid/HIDManager.h>
#include <ext/nanotime.h>
#include <filesystem/IOChannel.h>
#include <kpi/file.h>
#include <kpi/hid.h>


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

    de.name = "hid";
    de.uid = UID_ROOT;
    de.gid = GID_ROOT;
    de.perms = fs_perms_from_octal(0666);
    de.arg = 0;

    return Driver_Publish((DriverRef)self, &de);
}

// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
errno_t HIDDriver_read(HIDDriverRef _Nonnull self, IOChannelRef _Nonnull ioc, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    const bool isNonBlocking = (IOChannel_GetMode(ioc) & O_NONBLOCK) == O_NONBLOCK;
    const nanotime_t* timp = (isNonBlocking) ? &NANOTIME_ZERO : &NANOTIME_INF;
    HIDEvent* pe = buf;
    ssize_t nBytesRead = 0;

    while ((nBytesRead + sizeof(HIDEvent)) <= nBytesToRead) {
        // Only block waiting for the first event. For all other events we do not
        // wait.
        const errno_t e1 = HIDManager_GetNextEvent(gHIDManager, (pe == buf) ? timp : &NANOTIME_ZERO, pe);

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
            const nanotime_t* timeoutp = va_arg(ap, nanotime_t*);
            HIDEvent* evt = va_arg(ap, HIDEvent*);

            return HIDManager_GetNextEvent(gHIDManager, timeoutp, evt);
        }

        case kHIDCommand_FlushEvents:
            HIDManager_FlushEvents(gHIDManager);
            return EOK;
            
        case kHIDCommand_GetKeyRepeatDelays: {
            nanotime_t* initialp = va_arg(ap, nanotime_t*);
            nanotime_t* repeatp = va_arg(ap, nanotime_t*);

            HIDManager_GetKeyRepeatDelays(gHIDManager, initialp, repeatp);
            return EOK;
        }

        case kHIDCommand_SetKeyRepeatDelays: {
            const nanotime_t* initialp = va_arg(ap, nanotime_t*);
            const nanotime_t* repeatp = va_arg(ap, nanotime_t*);

            HIDManager_SetKeyRepeatDelays(gHIDManager, initialp, repeatp);
            return EOK;
        }

        case kHIDCommand_ObtainCursor: {
            return HIDManager_ObtainCursor(gHIDManager);
        }

        case kHIDCommand_ReleaseCursor:
            HIDManager_ReleaseCursor(gHIDManager);
            return EOK;

        case kHIDCommand_SetCursor: {
            const void** planes = va_arg(ap, const void**);
            const size_t bytesPerRow = va_arg(ap, size_t);
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const pixfmt_t format = va_arg(ap, pixfmt_t);
            const int hotSpotX = va_arg(ap, int);
            const int hotSpotY = va_arg(ap, int);

            return HIDManager_SetCursor(gHIDManager, planes, bytesPerRow, width, height, format, hotSpotX, hotSpotY);
        }

        case kHIDCommand_ShowCursor:
            HIDManager_ShowCursor(gHIDManager);
            return EOK;

        case kHIDCommand_HideCursor:
            HIDManager_HideCursor(gHIDManager);
            return EOK;

        case kHIDCommand_ObscureCursor:
            HIDManager_ObscureCursor(gHIDManager);
            return EOK;

        case kHIDCommand_ShieldCursor: {
            const int x = va_arg(ap, int);
            const int y = va_arg(ap, int);
            const int w = va_arg(ap, int);
            const int h = va_arg(ap, int);

            return HIDManager_ShieldMouseCursor(gHIDManager, x, y, w, h);
        }

        default:
            return super_n(ioctl, Driver, HIDDriver, self, ioc, cmd, ap);
    }
}


class_func_defs(HIDDriver, PseudoDriver,
override_func_def(onStart, HIDDriver, Driver)
override_func_def(read, HIDDriver, Driver)
override_func_def(ioctl, HIDDriver, Driver)
);
