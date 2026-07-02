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

    if ((err = IODriver_Create(class(ZorroDevice), 0, g_cats, (IODriverRef*)&self)) == EOK) {
        self->cfg = *config;
    }

    *pOutSelf = self;
    return err;
}

errno_t ZorroDevice_start(ZorroDeviceRef _Nonnull self)
{
    decl_try_err();
    IODriverRef dp;

    if (self->cfg.type == ZORRO_TYPE_RAM && self->cfg.start && self->cfg.logicalSize > 0) {
        err = ZRamDriver_Create(&dp);
        
        if (err == EOK) {
            err = IODriver_Launch(dp, (IODriverRef)self);
            Object_Release(dp);
        }
    }

    return err;
}

class_func_defs(ZorroDevice, IODriver,
override_func_def(start, ZorroDevice, IODriver)
);
