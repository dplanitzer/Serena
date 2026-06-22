//
//  LogHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef LogHandler_h
#define LogHandler_h

#include <handler/IODriverHandler.h>


open_class(LogHandler, IODriverHandler,
);
open_class_funcs(LogHandler, IODriverHandler,
);


extern errno_t LogHandler_Create(LogDriverRef _Nonnull drv, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* LogHandler_h */
