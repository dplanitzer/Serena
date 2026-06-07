//
//  PipeHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef PipeHandler_h
#define PipeHandler_h

#include <handler/Handler.h>


// Resource: PipeRef
open_class(PipeHandler, Handler,
);
open_class_funcs(PipeHandler, Handler,
);


extern errno_t PipeHandler_Create(PipeRef _Nonnull pipe, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutSelf);

#endif /* PipeHandler_h */
