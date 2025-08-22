//
//  PseudoDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/21/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "PseudoDriver.h"

IOCATS_DEF(g_cats, IOPSE_PSEUDO);


errno_t PseudoDriver_Create(Class* _Nonnull pClass, unsigned int options, DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_CreateRoot(pClass, options, g_cats, pOutSelf);
}

class_func_defs(PseudoDriver, Driver,
);
