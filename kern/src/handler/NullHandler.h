//
//  NullHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef NullHandler_h
#define NullHandler_h

#include <handler/PseudoHandler.h>


open_class(NullHandler, PseudoHandler,
);
open_class_funcs(NullHandler, PseudoHandler,
);


extern errno_t NullHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* NullHandler_h */
