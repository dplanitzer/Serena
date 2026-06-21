//
//  IODiskHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef IODiskHandler_h
#define IODiskHandler_h

#include <stdarg.h>
#include <handler/IODriverHandler.h>
#include <sched/mtx.h>


open_class(IODiskHandler, IODriverHandler,
    off_t   offset;
    mtx_t   mtx;
);
open_class_funcs(IODiskHandler, IODriverHandler,
);


extern errno_t IODiskHandler_Create(DiskDriverRef _Nonnull drv, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* IODiskHandler_h */
