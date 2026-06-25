//
//  ZStubDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZStubDriver.h"

IOCATS_DEF(g_cats, IOUNS_UNKNOWN);


errno_t ZStubDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(ZStubDriver), 0, g_cats, pOutSelf);
}

class_func_defs(ZStubDriver, Driver,
);
