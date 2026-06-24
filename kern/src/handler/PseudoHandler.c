//
//  PseudoHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/23/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "PseudoHandler.h"
#include <filesystem/Filesystem.h>
#include <filesystem/Inode.h>



errno_t PseudoHandler_Create(Class* _Nonnull pClass, int type, InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    struct PseudoHandler* self;

    err = Handler_Create(pClass, type, flags, (HandlerRef*)&self);
    if (err == EOK) {
        self->ino = Inode_Reacquire(ip);
        *pOutHandler = (HandlerRef)self;
    }

    return err;
}

void PseudoHandler_deinit(struct PseudoHandler* _Nonnull self)
{
    (void)Inode_Relinquish(self->ino);
    self->ino = NULL;
}


class_func_defs(PseudoHandler, Handler,
override_func_def(deinit, PseudoHandler, Object)
);
