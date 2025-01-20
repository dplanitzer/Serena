//
//  ZRamDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ZRamDriver_h
#define ZRamDriver_h

#include "ZorroDriver.h"


open_class(ZRamDriver, Driver,
);
open_class_funcs(ZRamDriver, Driver,
);

extern errno_t ZRamDriver_Create(DriverRef _Nullable parent, const ZorroConfiguration* _Nonnull config, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* ZRamDriver_h */
