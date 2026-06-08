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


open_class(InodeHandler, Handler,
    InodeRef _Nonnull   ino;
);
open_class_funcs(InodeHandler, Handler,

    // Returns the attributes of the Inode to which the channel is connected if
    // the channel is an Inode channel. Returns EBADF otherwise.
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*getAttributes)(void* _Nonnull self, fs_attr_t* _Nonnull attr);

    // Reduces or increases the size of a regular file if the channel is connected
    // to an Inode. Returns EBADF otherwise
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*truncate)(void* _Nonnull self, off_t length);
);


// Creates a file object.
extern errno_t InodeHandler_Create(InodeRef _Nonnull pNode, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutFile);

#define InodeHandler_GetAttributes(__self, __attr) \
invoke_n(getAttributes, InodeHandler, __self, __attr)

#define InodeHandler_Truncate(__self, __length) \
invoke_n(truncate, InodeHandler, __self, __length)

#endif /* InodeHandler_h */
