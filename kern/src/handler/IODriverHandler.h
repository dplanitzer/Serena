//
//  IODriverHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef IODriverHandler_h
#define IODriverHandler_h

#include <handler/Handler.h>


open_class(IODriverHandler, Handler,
    InodeRef _Nonnull   ino;
    DriverRef _Nonnull  driver;
);
open_class_funcs(IODriverHandler, Handler,
);

//XXX tmp
extern errno_t IONopHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

extern errno_t IODriverHandler_Create(Class* _Nonnull pClass, int type, InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);


//
// Subclassers
//

#define IODriverHandler_GetNode(__self) \
((struct IODriverHandler*)__self)->ino

#define IODriverHandler_GetDriver(__self) \
((void*) ((struct IODriverHandler*)__self)->driver)

#endif /* IODriverHandler_h */
