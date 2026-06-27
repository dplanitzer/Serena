//
//  ZorroBus.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ZorroBus.h"
#include "ZorroDevice.h"
#include "zorro_bus.h"


final_class_ivars(ZorroBus, Driver,
);

IOCATS_DEF(g_cats, IOBUS_ZORRO);


errno_t ZorroBus_Create(ZorroBusRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(ZorroBus), 0, g_cats, (DriverRef*)pOutSelf);
}

static void _auto_config_bus(ZorroBusRef _Nonnull _Locked self)
{
    decl_try_err();
    zorro_bus_t bus;
    size_t slotId = 0;

    // Auto config the Zorro bus
    zorro_auto_config(&bus);
    try(Driver_SetMaxChildCount((DriverRef)self, bus.count));


    // Create a ZorroDevice instance for each slot and start it
    deque_for_each(&bus.boards, zorro_board_t, it,
        const zorro_conf_t* cfg = &it->cfg;
        ZorroDeviceRef dp;
        
        if (ZorroDevice_Create(cfg, &dp) == EOK) {
            Driver_AttachStartChild((DriverRef)self, (DriverRef)dp, slotId++);
            Object_Release(dp);
        }
    )

catch:
    zorro_destroy_bus(&bus);
}

errno_t ZorroBus_onStart(ZorroBusRef _Nonnull _Locked self)
{
    // Auto-config the bus. Discover as many cards as possible and ignore anything
    // that fails.
    _auto_config_bus(self);

    return EOK;
}


class_func_defs(ZorroBus, Driver,
override_func_def(onStart, ZorroBus, Driver)
);
