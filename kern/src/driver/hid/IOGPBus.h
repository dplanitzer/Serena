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

#include <driver/Driver.h>


typedef errno_t (*IOGPCreateDriverFunc)(void* _Nonnull ctx, int port, int type, DriverRef _Nullable * _Nonnull pOutDriver);

open_class(IOGPBus, Driver,
    mtx_t                           mtx;
    IOGPCreateDriverFunc _Nonnull   createHidDevice;
    void* _Nullable                 ctx;
);
open_class_funcs(IOGPBus, Driver,
);


extern errno_t IOGPBus_Create(IOGPCreateDriverFunc _Nonnull func, void* _Nullable ctx, IOGPBusRef _Nullable * _Nonnull pOutSelf);

extern errno_t IOGPBus_GetPortDevice(IOGPBusRef _Nonnull self, int port, int* _Nullable pOutType, did_t* _Nullable pOutId);
extern errno_t IOGPBus_SetPortDevice(IOGPBusRef _Nonnull self, int port, int type);
extern errno_t IOGPBus_GetPortForDeviceId(IOGPBusRef _Nonnull self, did_t id, int* _Nonnull pOutPort);

#endif /* __IOGPBUS__ > 0 */

#endif /* IOGPBus_h */
