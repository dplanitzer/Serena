//
//  NullHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef NullHandler_h
#define NullHandler_h

#include <handler/Handler.h>


final_class(NullHandler, Handler);

extern errno_t NullHandler_Create(HandlerRef _Nullable * _Nonnull pOutSelf);

#endif /* NullHandler_h */
