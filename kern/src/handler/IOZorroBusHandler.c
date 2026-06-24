//
//  IOZorroBusHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IOZorroBusHandler.h"
#include <driver/hw/m68k-amiga/zorro/ZorroController.h>


errno_t IOZorroBusHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return IODriverHandler_Create(class(IOZorroBusHandler), FD_TYPE_DEVICE, ip, flags, pOutHandler);
}

errno_t IOZorroBusHandler_control(struct IOZorroBusHandler* _Nonnull self, int cmd, va_list ap)
{
    ZorroControllerRef bus = IODriverHandler_GetDriver(self);

    switch (cmd) {
        case kZorroCommand_GetCardCount: {
            size_t* pCount = va_arg(ap, size_t*);
            
            *pCount = ZorroController_GetCardCount(bus);
            return EOK;
        }

        case kZorroCommand_GetCardConfig: {
            size_t idx = va_arg(ap, size_t);
            zorro_conf_t* pcfg = va_arg(ap, zorro_conf_t*);

            return ZorroController_GetCardConfig(bus, idx, pcfg);
        }

        default:
        return Handler_Super_Control(IOZorroBusHandler, cmd, ap);
    }
}


class_func_defs(IOZorroBusHandler, IODriverHandler,
override_func_def(control, IOZorroBusHandler, Handler)
);
