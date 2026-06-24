//
//  IODriverHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IODriverHandler.h"
#include <driver/Driver.h>
#include <ext/math.h>
#include <filesystem/Filesystem.h>
#include <filesystem/Inode.h>



//XXX tmp
errno_t IONopHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return IODriverHandler_Create(class(IODriverHandler), FD_TYPE_DRIVER, ip, flags, pOutHandler);
}

errno_t IODriverHandler_Create(Class* _Nonnull pClass, int type, InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    DriverRef drv = (DriverRef)Inode_GetResource(ip);
    struct IODriverHandler* self;

    err = Driver_Open(drv, flags);
    if (err == EOK) {
        err = Handler_Create(pClass, type, flags, (HandlerRef*)&self);
        if (err != EOK) {
            Driver_Close(drv);
            return err;
        }

        self->ino = Inode_Reacquire(ip);
        self->driver = drv;
        *pOutHandler = (HandlerRef)self;
    }

    return err;
}

void IODriverHandler_deinit(struct IODriverHandler* _Nonnull self)
{
    Driver_Close(self->driver);
    self->driver = NULL;

    (void)Inode_Relinquish(self->ino);
    self->ino = NULL;
}

errno_t IODriverHandler_control(struct IODriverHandler* _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kDriverCommand_GetId: {
            did_t* pdid = va_arg(ap, did_t*);

            *pdid = Driver_GetId(self->driver);
            return EOK;
        }

        case kDriverCommand_GetCategories: {
            iocat_t* buf = va_arg(ap, iocat_t*);
            const size_t bufsiz = va_arg(ap, size_t);
            const iocat_t* pcats = Driver_GetCategories(self->driver);
            const iocat_t* sp = pcats;
            size_t i, len = 0;

            if (bufsiz == 0) {
                return EINVAL;
            }

            while (*sp != IOCAT_END) {
                sp++; len++;
            }

            if (bufsiz < len + 1) {
                return ERANGE;
            }

            for (i = 0; i < __min(len, bufsiz - 1); i++) {
                buf[i] = pcats[i];
            }
            buf[i] = IOCAT_END;

            return EOK;
        }

        default:
            return ENOTIOCTLCMD;
    }
}


class_func_defs(IODriverHandler, Handler,
override_func_def(deinit, IODriverHandler, Object)
override_func_def(control, IODriverHandler, Handler)
);
