//
//  DriverHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverHandler_h
#define DriverHandler_h

#include <handler/Handler.h>
#include <sched/mtx.h>


// Resource: DriverRef
open_class(DriverHandler, Handler,
    void* _Nullable     extras;
    mtx_t               ser_mtx;
);
open_class_funcs(DriverHandler, Handler,
);


extern errno_t DriverHandler_Create(DriverRef _Nonnull drv, int channelType, unsigned int mode, size_t nExtraBytes, HandlerRef _Nullable * _Nonnull pOutHandler);

#define DriverHandler_GetExtrasAs(__self, __type) \
((__type*)((DriverHandlerRef)__self)->extras)

#endif /* DriverHandler_h */
