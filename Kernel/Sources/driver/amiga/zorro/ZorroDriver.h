//
//  ZorroDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ZorroDriver_h
#define ZorroDriver_h

#include <driver/Driver.h>
#include <System/Zorro.h>


open_class(ZorroDriver, Driver,
    const ZorroConfiguration* _Nonnull boardConfig;
);
open_class_funcs(ZorroDriver, Driver,
);

// Create a driver instance. 
#define ZorroDriver_Create(__className, __options, __parent, __config, __pOutDriver) \
    _ZorroDriver_Create(&k##__className##Class, __options, __parent, __config, (DriverRef*)__pOutDriver)

#define ZorroDriver_GetBoardConfiguration(__self) \
(((ZorroDriverRef)__self)->boardConfig)


extern errno_t _ZorroDriver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable parent, const ZorroConfiguration* _Nonnull config, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* ZorroDriver_h */
