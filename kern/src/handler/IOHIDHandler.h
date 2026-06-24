//
//  IOHIDHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef IOHIDHandler_h
#define IOHIDHandler_h

#include <stdarg.h>
#include <handler/PseudoHandler.h>


open_class(IOHIDHandler, PseudoHandler,
);
open_class_funcs(IOHIDHandler, PseudoHandler,
);


extern errno_t IOHIDHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* IOHIDHandler_h */
