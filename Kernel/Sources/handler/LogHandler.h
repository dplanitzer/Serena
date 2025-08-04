//
//  LogHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef LogHandler_h
#define LogHandler_h

#include <handler/Handler.h>


final_class(LogHandler, Handler);

extern errno_t LogHandler_Create(HandlerRef _Nullable * _Nonnull pOutSelf);

#endif /* LogHandler_h */
