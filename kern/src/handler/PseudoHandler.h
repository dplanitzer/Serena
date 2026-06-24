//
//  PseudoHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/23/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef PseudoHandler_h
#define PseudoHandler_h

#include <handler/Handler.h>


open_class(PseudoHandler, Handler,
    InodeRef _Nonnull   ino;
);
open_class_funcs(PseudoHandler, Handler,
);


extern errno_t PseudoHandler_Create(Class* _Nonnull pClass, int type, InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);


//
// Subclassers
//

#define PseudoHandler_GetNode(__self) \
((struct PseudoHandler*)__self)->ino

#endif /* PseudoHandler_h */
