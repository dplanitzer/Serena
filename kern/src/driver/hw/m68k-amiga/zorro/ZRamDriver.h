//
//  ZRamDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ZRamDriver_h
#define ZRamDriver_h

#include <driver/IODriver.h>


open_class(ZRamDriver, IODriver,
);
open_class_funcs(ZRamDriver, IODriver,
);


extern errno_t ZRamDriver_Create(IODriverRef _Nullable * _Nonnull pOutSelf);

extern size_t ZRamDriver_GetMemorySize(ZRamDriverRef _Nonnull self);

#endif /* ZRamDriver_h */
