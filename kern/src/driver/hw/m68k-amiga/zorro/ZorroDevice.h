//
//  ZorroDevice.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ZorroDevice_h
#define ZorroDevice_h

#include <driver/Driver.h>
#include <machine/amiga/zorro.h>


open_class(ZorroDevice, Driver,
    zorro_conf_t    cfg;
);
open_class_funcs(ZorroDevice, Driver,
);

// Create a driver instance. 
extern errno_t ZorroDevice_Create(const zorro_conf_t* _Nonnull config, ZorroDeviceRef _Nullable * _Nonnull pOutSelf);

#define ZorroDevice_GetConfiguration(__self) \
((const zorro_conf_t*)&((ZorroDeviceRef)(__self))->cfg)

#endif /* ZorroDevice_h */
