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


open_class(ZRamDriver, ZorroDriver,
);
open_class_funcs(ZRamDriver, ZorroDriver,
);

extern errno_t ZRamDriver_Create(DriverRef _Nullable parent, const zorro_conf_t* _Nonnull config, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* ZRamDriver_h */
