//
//  ZStubDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ZStubDriver_h
#define ZStubDriver_h

#include "ZorroDriver.h"


open_class(ZStubDriver, Driver,
    ZorroDriverRef _Nonnull _Weak   card;
);
open_class_funcs(ZStubDriver, Driver,
);
class_ref(ZStubDriver);

extern errno_t ZStubDriver_Create(ZorroDriverRef _Nonnull zdp, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* ZStubDriver_h */
