//
//  DriverHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/4/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverHandler_h
#define DriverHandler_h

#include <handler/Handler.h>


open_class(DriverHandler, Handler,
    DriverRef _Nonnull  driver;
);
open_class_funcs(DriverHandler, Handler,
);


extern errno_t DriverHandler_Create(Class* _Nonnull pClass, DriverRef _Nonnull driver, HandlerRef _Nullable * _Nonnull pOutSelf);

//
// Subclassers
//

#define DriverHandler_GetDriver(__self) \
((DriverHandlerRef)__self)->driver

#endif /* DriverHandler_h */
