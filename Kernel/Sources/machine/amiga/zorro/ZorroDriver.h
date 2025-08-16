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
    zorro_conf_t    cfg;
);
open_class_funcs(ZorroDriver, Driver,
);

// Create a driver instance. 
extern errno_t ZorroDriver_Create(const zorro_conf_t* _Nonnull config, CatalogId parentDirId, ZorroDriverRef _Nullable * _Nonnull pOutSelf);

#define ZorroDriver_GetConfiguration(__self) \
((const zorro_conf_t*)&((ZorroDriverRef)__self)->cfg)

#endif /* ZorroDriver_h */
