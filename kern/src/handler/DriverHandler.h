//
//  DriverHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverHandler_h
#define DriverHandler_h

#include <stdarg.h>
#include <handler/Handler.h>
#include <sched/mtx.h>


open_class(DriverHandler, Handler,
    off_t               offset;
    DriverRef _Nonnull  driver;
    mtx_t               ser_mtx;
);
open_class_funcs(DriverHandler, Handler,
);


extern errno_t DriverHandler_Create(DriverRef _Nonnull drv, int channelType, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutHandler);

#define DriverHandler_GetDriverAs(__self, __class) \
((__class##Ref) ((DriverHandlerRef)__self)->driver)

#endif /* DriverHandler_h */
