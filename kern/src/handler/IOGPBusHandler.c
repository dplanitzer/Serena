//
//  IOGPBusHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IOGPBusHandler.h"
#include <driver/hw/m68k-amiga/hid/GamePortController.h>


errno_t IOGPBusHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return IODriverHandler_Create(class(IOGPBusHandler), FD_TYPE_DEVICE, ip, flags, pOutHandler);
}

errno_t IOGPBusHandler_control(struct IOGPBusHandler* _Nonnull self, int cmd, va_list ap)
{
    GamePortControllerRef bus = IODriverHandler_GetDriver(self);

    switch (cmd) {
        case kGamePortCommand_GetPortDevice: {
            const int port = va_arg(ap, int);
            int* ptype = va_arg(ap, int*);
            did_t* pdid = va_arg(ap, did_t*);

            return GamePortController_GetPortDevice(bus, port, ptype, pdid);
        }

        case kGamePortCommand_SetPortDevice: {
            const int port = va_arg(ap, int);
            const int itype = va_arg(ap, int);

            return GamePortController_SetPortDevice(bus, port, itype);
        }

        case kGamePortCommand_GetPortForDriver: {
            const did_t did = va_arg(ap, did_t);
            int* pport = va_arg(ap, int*);

            return GamePortController_GetPortForDriver(bus, did, pport);
        }

        default:
            return Handler_Super_Control(IOGPBusHandler, cmd, ap);
    }
}


class_func_defs(IOGPBusHandler, IODriverHandler,
override_func_def(control, IOGPBusHandler, Handler)
);
