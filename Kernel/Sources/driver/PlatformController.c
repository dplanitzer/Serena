//
//  PlatformController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PlatformController.h"


errno_t PlatformController_Create(Class* _Nonnull pClass, DriverCatalogId baseBusId, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    PlatformControllerRef self;

    err = Driver_Create(pClass, 0, NULL, (DriverRef*)&self);
    if (err == EOK) {
        self->baseBusId = baseBusId;
    }

    *pOutSelf = (DriverRef)self;
    return err;
}

DriverCatalogId PlatformController_getParentBusCatalogId(DriverRef _Nonnull _Locked self)
{
    return self->busCatalogId;
}

class_func_defs(PlatformController, Driver,
override_func_def(getParentBusCatalogId, PlatformController, Driver)
);
