//
//  IOGraphicsHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef IOGraphicsHandler_h
#define IOGraphicsHandler_h

#include <stdarg.h>
#include <handler/IODriverHandler.h>


open_class(IOGraphicsHandler, IODriverHandler,
);
open_class_funcs(IOGraphicsHandler, IODriverHandler,
);


extern errno_t IOGraphicsHandler_Create(GraphicsDriverRef _Nonnull drv, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* IOGraphicsHandler_h */
