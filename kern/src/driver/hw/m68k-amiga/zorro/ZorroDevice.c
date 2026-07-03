//
//  ZorroDevice.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZorroDevice.h"
#include "ZRamDriver.h"
#include "zorro_bus.h"

IOCATS_NONE(g_cats);


errno_t ZorroDevice_Create(const zorro_conf_t* _Nonnull config, ZorroDeviceRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ZorroDeviceRef self;

    if ((err = IODriver_Create(class(ZorroDevice), g_cats, (IODriverRef*)&self)) == EOK) {
        self->cfg = *config;
    }

    *pOutSelf = self;
    return err;
}

void ZorroDevice_onLaunched(ZorroDeviceRef _Nonnull self)
{
    IODriverRef dp;

    if (self->cfg.type == ZORRO_TYPE_RAM && self->cfg.start && self->cfg.logicalSize > 0) {
        if (ZRamDriver_Create(&dp) == EOK) {
            IODriver_Launch(dp, (IODriverRef)self);
            Object_Release(dp);
        }
    }
}

class_func_defs(ZorroDevice, IODriver,
override_func_def(onLaunched, ZorroDevice, IODriver)
);
