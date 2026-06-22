//
//  NullHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef NullHandler_h
#define NullHandler_h

#include <handler/IODriverHandler.h>


open_class(NullHandler, IODriverHandler,
);
open_class_funcs(NullHandler, IODriverHandler,
);


extern errno_t NullHandler_Create(NullDriverRef _Nonnull drv, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* NullHandler_h */
