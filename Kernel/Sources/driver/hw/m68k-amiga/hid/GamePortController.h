//
//  GamePortController.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef GamePortController_h
#define GamePortController_h

#include <driver/Driver.h>
#include <kpi/hid.h>


open_class(GamePortController, Driver,
    mtx_t       io_mtx;
);
open_class_funcs(GamePortController, Driver,
    
    // Invoked when the game port controller is instructed that an input device
    // of type 'type' is connected to the port 'port'. Overrides should create
    // an instance of a suitable driver and return it. An ENODEV error should
    // be returned if no suitable driver is available. 
    // 
    // Override: Optional
    // Default Behavior: Returns a suitable driver instance
    errno_t (*createInputDriver)(void* _Nonnull self, int port, int type, DriverRef _Nullable * _Nonnull pOutDriver);
);


extern errno_t GamePortController_Create(GamePortControllerRef _Nullable * _Nonnull pOutSelf);

#define GamePortController_CreateInputDriver(__self, __port, __type, __pOutDriver) \
invoke_n(createInputDriver, GamePortController, __self, __port, __type, __pOutDriver)

#endif /* GamePortController_h */
