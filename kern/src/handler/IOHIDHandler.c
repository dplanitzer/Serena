//
//  IOHIDHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IOHIDHandler.h"
#include <driver/hid/IOHIDManager.h>
#include <ext/nanotime.h>


errno_t IOHIDHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return PseudoHandler_Create(class(IOHIDHandler), FD_TYPE_DEVICE, ip, flags, pOutHandler);
}

// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
errno_t IOHIDHandler_read(struct IOHIDHandler* _Nonnull self, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    const fd_flags_t flags = Handler_GetFlags(self);
    const bool isNonBlocking = (flags & O_NONBLOCK) == O_NONBLOCK;
    const nanotime_t* timp = (isNonBlocking) ? &NANOTIME_ZERO : &NANOTIME_INF;
    hid_event_t* pe = buf;
    ssize_t nBytesRead = 0;

    if ((flags & O_RDONLY) == 0) {
        return EBADF;
    }


    while ((nBytesRead + sizeof(hid_event_t)) <= nBytesToRead) {
        // Only block waiting for the first event. For all other events we do not
        // wait.
        const errno_t e1 = IOHIDManager_GetNextEvent(gIOHIDManager, (pe == buf) ? timp : &NANOTIME_ZERO, pe);

        if (e1 != EOK) {
            // Return with an error if we were not able to read any event data at
            // all and return with the data we were able to read otherwise.
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        nBytesRead += sizeof(hid_event_t);
        pe++;
    }

    *nOutBytesRead = nBytesRead;
    return err;
}

errno_t IOHIDHandler_control(struct IOHIDHandler* _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case HID_CMD_GET_EVENT: {
            const nanotime_t* timeoutp = va_arg(ap, nanotime_t*);
            hid_event_t* evt = va_arg(ap, hid_event_t*);

            return IOHIDManager_GetNextEvent(gIOHIDManager, timeoutp, evt);
        }

        case HID_CMD_FLUSH_EVENTS:
            IOHIDManager_FlushEvents(gIOHIDManager);
            return EOK;
            
        case HID_CMD_KEY_REPEAT_DELAYS: {
            nanotime_t* initialp = va_arg(ap, nanotime_t*);
            nanotime_t* repeatp = va_arg(ap, nanotime_t*);

            IOHIDManager_GetKeyRepeatDelays(gIOHIDManager, initialp, repeatp);
            return EOK;
        }

        case HID_CMD_SET_KEY_REPEAT_DELAYS: {
            const nanotime_t* initialp = va_arg(ap, nanotime_t*);
            const nanotime_t* repeatp = va_arg(ap, nanotime_t*);

            IOHIDManager_SetKeyRepeatDelays(gIOHIDManager, initialp, repeatp);
            return EOK;
        }

        case HID_CMD_OBTAIN_CURSOR: {
            return IOHIDManager_ObtainCursor(gIOHIDManager);
        }

        case HID_CMD_RELEASE_CURSOR:
            IOHIDManager_ReleaseCursor(gIOHIDManager);
            return EOK;

        case HID_CMD_SET_CURSOR: {
            const void** planes = va_arg(ap, const void**);
            const size_t bytesPerRow = va_arg(ap, size_t);
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const vio_pixfmt_t format = va_arg(ap, vio_pixfmt_t);
            const int hotSpotX = va_arg(ap, int);
            const int hotSpotY = va_arg(ap, int);

            return IOHIDManager_SetCursor(gIOHIDManager, planes, bytesPerRow, width, height, format, hotSpotX, hotSpotY);
        }

        case HID_CMD_SHOW_CURSOR:
            IOHIDManager_ShowCursor(gIOHIDManager);
            return EOK;

        case HID_CMD_HIDE_CURSOR:
            IOHIDManager_HideCursor(gIOHIDManager);
            return EOK;

        case HID_CMD_OBSCURE_CURSOR:
            IOHIDManager_ObscureCursor(gIOHIDManager);
            return EOK;

        case HID_CMD_SHIELD_CURSOR: {
            const int x = va_arg(ap, int);
            const int y = va_arg(ap, int);
            const int w = va_arg(ap, int);
            const int h = va_arg(ap, int);

            return IOHIDManager_ShieldMouseCursor(gIOHIDManager, x, y, w, h);
        }

#if __IOGPBUS__ > 0
        case HID_CMD_PORT_COUNT: {
            size_t* pcount = va_arg(ap, size_t*);

            *pcount = IOHIDManager_GetPortCount(gIOHIDManager);
            return EOK;
        }

        case HID_CMD_PORT_DEVICE: {
            const int port = va_arg(ap, int);
            int* ptype = va_arg(ap, int*);
            did_t* pdid = va_arg(ap, did_t*);

            return IOHIDManager_GetPortDevice(gIOHIDManager, port, ptype, pdid);
        }

        case HID_CMD_SET_PORT_DEVICE: {
            const int port = va_arg(ap, int);
            const int itype = va_arg(ap, int);

            return IOHIDManager_SetPortDevice(gIOHIDManager, port, itype);
        }

        case HID_CMD_PORT_FOR_DEVICE: {
            const did_t did = va_arg(ap, did_t);
            int* pport = va_arg(ap, int*);

            return IOHIDManager_GetPortForDeviceId(gIOHIDManager, did, pport);
        }
#endif

        default:
            return Handler_Super_Control(IOHIDHandler, cmd, ap);
    }
}


class_func_defs(IOHIDHandler, PseudoHandler,
override_func_def(read, IOHIDHandler, Handler)
override_func_def(control, IOHIDHandler, Handler)
);
