//
//  ConsoleHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef ConsoleHandler_h
#define ConsoleHandler_h

#include <handler/PseudoHandler.h>


open_class(ConsoleHandler, PseudoHandler,
);
open_class_funcs(ConsoleHandler, PseudoHandler,
);


extern errno_t ConsoleHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler);

#endif /* ConsoleHandler_h */
