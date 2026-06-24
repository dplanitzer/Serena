//
//  IOGPBusHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef IOGPBusHandler_h
#define IOGPBusHandler_h

#include <stdarg.h>
#include <handler/IODriverHandler.h>


open_class(IOGPBusHandler, IODriverHandler,
);
open_class_funcs(IOGPBusHandler, IODriverHandler,
);


extern errno_t IOGPBusHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* IOGPBusHandler_h */
