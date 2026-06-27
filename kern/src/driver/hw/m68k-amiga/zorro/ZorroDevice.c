//
//  ZorroDevice.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZorroDevice.h"
#include "ZRamDriver.h"
#include "ZStubDriver.h"
#include "zorro_bus.h"

IOCATS_NONE(g_cats);


errno_t ZorroDevice_Create(const zorro_conf_t* _Nonnull config, ZorroDeviceRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ZorroDeviceRef self;

    if ((err = Driver_Create(class(ZorroDevice), 0, g_cats, (DriverRef*)&self)) == EOK) {
        Driver_SetMaxChildCount((DriverRef)self, 1);
        self->cfg = *config;
    }

    *pOutSelf = self;
    return err;
}

errno_t ZorroDevice_onStart(ZorroDeviceRef _Nonnull _Locked self)
{
    decl_try_err();
    DriverRef dp = NULL;

    if (self->cfg.type == ZORRO_TYPE_RAM && self->cfg.start && self->cfg.logicalSize > 0) {
        err = ZRamDriver_Create(&dp);
    }
    else {
        err = ZStubDriver_Create(&dp);
    }


    if (dp) {
        err = Driver_AttachStartChild((DriverRef)self, dp, 0);
        Object_Release(dp);
    }

    return err;
}

class_func_defs(ZorroDevice, Driver,
override_func_def(onStart, ZorroDevice, Driver)
);
