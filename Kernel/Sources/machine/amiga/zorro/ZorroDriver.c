//
//  ZorroDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZorroDriver.h"
#include "ZRamDriver.h"
#include "zorro_bus.h"


errno_t ZorroDriver_Create(const zorro_conf_t* _Nonnull config, CatalogId parentDirId, ZorroDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ZorroDriverRef self;

    if ((err = Driver_Create(class(ZorroDriver), 0, parentDirId, (DriverRef*)&self)) == EOK) {
        self->cfg = *config;
    }

    *pOutSelf = self;
    return err;
}

errno_t ZorroDriver_onStart(ZorroDriverRef _Nonnull _Locked self)
{
    if (self->cfg.type == ZORRO_TYPE_RAM && self->cfg.start && self->cfg.logicalSize > 0) {
        DriverRef dp;
        
        if (ZRamDriver_Create(self, &dp) == EOK) {
            Driver_StartAdoptChild((DriverRef)self, dp);
        }
    }

    return EOK;
}

class_func_defs(ZorroDriver, Driver,
override_func_def(onStart, ZorroDriver, Driver)
);
