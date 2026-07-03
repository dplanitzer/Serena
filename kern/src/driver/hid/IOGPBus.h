//
//  IOGPBus.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef IOGPBus_h
#define IOGPBus_h

#if __IOGPBUS__ > 0

#include <driver/IODriver.h>


open_class(IOGPBus, IODriver,
    mtx_t   mtx;
    did_t   portDriverId[__IOGPBUS__];
    char    portType[__IOGPBUS__];
);
open_class_funcs(IOGPBus, IODriver,
    // Invoked when the controller type for a bus port changes. Overrides should
    // create a suitable driver based on 'type' and return it in 'pOutDriver'.
    // The port type switch will fail if this function returns an error.
    errno_t (*createPortDriver)(void* _Nonnull self, int port, int type, IODriverRef _Nullable * _Nonnull pOutDriver);
);


extern errno_t IOGPBus_Create(Class* _Nonnull pClass, IOGPBusRef _Nullable * _Nonnull pOutSelf);

extern size_t IOGPBus_GetPortCount(IOGPBusRef _Nonnull self);
extern errno_t IOGPBus_GetPortDevice(IOGPBusRef _Nonnull self, int port, int* _Nullable pOutType, did_t* _Nullable pOutId);
extern errno_t IOGPBus_SetPortDevice(IOGPBusRef _Nonnull self, int port, int type);
extern errno_t IOGPBus_GetPortForDeviceId(IOGPBusRef _Nonnull self, did_t id, int* _Nonnull pOutPort);


//
// Subclassers
//

#define IOGPBus_CreatePortDriver(__self, __port, __type, __outDriver) \
invoke_n(createPortDriver, IOGPBus, __self, __port, __type, __outDriver)

#endif /* __IOGPBUS__ > 0 */

#endif /* IOGPBus_h */
