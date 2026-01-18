//
//  ZorroController.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ZorroController.h"
#include "ZorroDriver.h"
#include "zorro_bus.h"


final_class_ivars(ZorroController, Driver,
);

IOCATS_DEF(g_cats, IOBUS_ZORRO);


errno_t ZorroController_Create(ZorroControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(ZorroController), kDriver_IsBus, g_cats, (DriverRef*)pOutSelf);
}

static void _auto_config_bus(ZorroControllerRef _Nonnull _Locked self)
{
    decl_try_err();
    zorro_bus_t bus;
    size_t slotId = 0;

    // Auto config the Zorro bus
    zorro_auto_config(&bus);
    try(Driver_SetMaxChildCount((DriverRef)self, bus.count));


    // Create a ZorroDriver instance for each slot and start it
    deque_for_each(&bus.boards, zorro_board_t, it,
        const zorro_conf_t* cfg = &it->cfg;
        ZorroDriverRef dp;
        
        if (ZorroDriver_Create(cfg, &dp) == EOK) {
            Driver_AttachStartChild((DriverRef)self, (DriverRef)dp, slotId++);
            Object_Release(dp);
        }
    )

catch:
    zorro_destroy_bus(&bus);
}

errno_t ZorroController_onStart(ZorroControllerRef _Nonnull _Locked self)
{
    decl_try_err();

    DirEntry be;
    be.name = "zorro-bus";
    be.uid = kUserId_Root;
    be.gid = kGroupId_Root;
    be.perms = perm_from_octal(0755);

    DriverEntry de;
    de.name = "self";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.arg = 0;

    try(Driver_PublishBus((DriverRef)self, &be, &de));


    // Auto-config the bus. Discover as many cards as possible and ignore anything
    // that fails.
    _auto_config_bus(self);

catch:
    return err;
}

errno_t ZorroController_ioctl(ZorroControllerRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kZorroCommand_GetCardCount: {
            size_t* pCount = va_arg(ap, size_t*);
            
            *pCount = Driver_GetChildCount((DriverRef)self);
            return EOK;
        }

        case kZorroCommand_GetCardConfig: {
            size_t idx = va_arg(ap, size_t);
            zorro_conf_t* pcfg = va_arg(ap, zorro_conf_t*);
            ZorroDriverRef zdp = (ZorroDriverRef)Driver_GetChildAt((DriverRef)self, idx);
        
            if (zdp) {
                *pcfg = *ZorroDriver_GetConfiguration(zdp);
                return EOK;
            }
            else {
                return EINVAL;
            }
        }

        default:
            return super_n(ioctl, Driver, ZorroController, self, pChannel, cmd, ap);
    }
}


class_func_defs(ZorroController, Driver,
override_func_def(onStart, ZorroController, Driver)
override_func_def(ioctl, ZorroController, Driver)
);
