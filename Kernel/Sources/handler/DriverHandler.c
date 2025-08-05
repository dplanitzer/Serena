//
//  DriverHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/4/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DriverHandler.h"


errno_t DriverHandler_Create(Class* _Nonnull pClass, DriverRef _Nonnull driver, HandlerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverHandlerRef self;

    err = Object_Create(pClass, 0, (void**)&self);
    if (err == EOK) {
        self->driver = Object_RetainAs(driver, Driver);
    }

    *pOutSelf = (HandlerRef)self;
    return err;
}

void DriverHandler_deinit(DriverHandlerRef _Nonnull self)
{
    Object_Release(self->driver);
    self->driver = NULL;
}

class_func_defs(DriverHandler, Handler,
override_func_def(deinit, DriverHandler, Object)
);
