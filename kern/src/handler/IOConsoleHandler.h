//
//  IOConsoleHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef IOConsoleHandler_h
#define IOConsoleHandler_h

#include <stdarg.h>
#include <handler/IODriverHandler.h>


open_class(IOConsoleHandler, IODriverHandler,
);
open_class_funcs(IOConsoleHandler, IODriverHandler,
);


extern errno_t IOConsoleHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* IOConsoleHandler_h */
