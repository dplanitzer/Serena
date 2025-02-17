//
//  ZorroDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZorroDriver.h"


errno_t ZorroDriver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable parent, const ZorroConfiguration* _Nonnull config, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ZorroDriverRef self;

    if ((err = Driver_Create(pClass, options, parent, (DriverRef*)&self)) == EOK) {
        self->boardConfig = config;
    }

    *pOutSelf = (DriverRef)self;
    return err;
}

class_func_defs(ZorroDriver, Driver,
);
