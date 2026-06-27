//
//  ZRamDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ZRamDriver_h
#define ZRamDriver_h

#include "ZorroDevice.h"


open_class(ZRamDriver, Driver,
);
open_class_funcs(ZRamDriver, Driver,
);


extern errno_t ZRamDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf);

extern size_t ZRamDriver_GetMemorySize(ZRamDriverRef _Nonnull self);

#endif /* ZRamDriver_h */
