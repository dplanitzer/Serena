//
//  InodeHandler.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef InodeHandler_h
#define InodeHandler_h

#include <handler/Handler.h>


// Resource: InodeRef
open_class(InodeHandler, Handler,
);
open_class_funcs(InodeHandler, Handler,
);


// Creates a file object.
extern errno_t InodeHandler_Create(InodeRef _Nonnull pNode, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutFile);

#endif /* InodeHandler_h */
