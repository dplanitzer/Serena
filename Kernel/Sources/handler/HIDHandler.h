//
//  HIDHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDHandler_h
#define HIDHandler_h

#include <handler/Handler.h>


final_class(HIDHandler, Handler);

extern errno_t HIDHandler_Create(HandlerRef _Nullable * _Nonnull pOutSelf);

#endif /* HIDHandler_h */
