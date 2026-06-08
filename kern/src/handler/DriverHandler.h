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
    mtx_t   ser_mtx;
);
open_class_funcs(DriverHandler, Handler,
);


extern errno_t DriverHandler_Create(DriverRef _Nonnull drv, int channelType, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* DriverHandler_h */
