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
#include <machine/amiga/zorro.h>


open_class(ZorroDriver, Driver,
    const zorro_conf_t* _Nonnull boardConfig;
);
open_class_funcs(ZorroDriver, Driver,
);

// Create a driver instance. 
extern errno_t ZorroDriver_Create(Class* _Nonnull pClass, unsigned options, CatalogId parentDirId, const zorro_conf_t* _Nonnull config, DriverRef _Nullable * _Nonnull pOutSelf);

#define ZorroDriver_GetBoardConfiguration(__self) \
(((ZorroDriverRef)__self)->boardConfig)

#endif /* ZorroDriver_h */
