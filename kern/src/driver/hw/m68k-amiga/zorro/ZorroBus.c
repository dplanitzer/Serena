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


final_class_ivars(ZorroBus, IODriver,
);

IOCATS_DEF(g_cats, IOBUS_ZORRO);


errno_t ZorroBus_Create(ZorroBusRef _Nullable * _Nonnull pOutSelf)
{
    return IODriver_Create(class(ZorroBus), 0, g_cats, (IODriverRef*)pOutSelf);
}

static void ZorroBus_ScanBus(ZorroBusRef _Nonnull self)
{
    decl_try_err();
    zorro_bus_t bus;
    size_t slotId = 0;

    // Auto config the Zorro bus
    zorro_auto_config(&bus);


    // Create a ZorroDevice instance for each slot and start it
    deque_for_each(&bus.boards, zorro_board_t, it,
        const zorro_conf_t* cfg = &it->cfg;
        ZorroDeviceRef dp;
        
        if (ZorroDevice_Create(cfg, &dp) == EOK) {
            IODriver_Launch((IODriverRef)dp, (IODriverRef)self);
            Object_Release(dp);
        }
    )

catch:
    zorro_destroy_bus(&bus);
}

void ZorroBus_onLaunched(ZorroBusRef _Nonnull self)
{
    // Auto-config the bus. Discover as many cards as possible and ignore anything
    // that fails.
    ZorroBus_ScanBus(self);
}


class_func_defs(ZorroBus, IODriver,
override_func_def(onLaunched, ZorroBus, IODriver)
);
