//
//  LogHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef LogHandler_h
#define LogHandler_h

#include <handler/PseudoHandler.h>


open_class(LogHandler, PseudoHandler,
);
open_class_funcs(LogHandler, PseudoHandler,
);


extern errno_t LogHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* LogHandler_h */
