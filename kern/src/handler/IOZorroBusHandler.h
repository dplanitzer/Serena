//
//  IOZorroBusHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef IOZorroBusHandler_h
#define IOZorroBusHandler_h

#include <stdarg.h>
#include <handler/IODriverHandler.h>


open_class(IOZorroBusHandler, IODriverHandler,
);
open_class_funcs(IOZorroBusHandler, IODriverHandler,
);


extern errno_t IOZorroBusHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* IOZorroBusHandler_h */
