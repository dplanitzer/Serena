//
//  NullDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "NullDriver.h"


final_class_ivars(NullDriver, PseudoDriver,
);


errno_t NullDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return PseudoDriver_Create(class(NullDriver), 0, pOutSelf);
}

errno_t NullDriver_onStart(NullDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    DriverEntry de;

    de.dirId = Driver_GetBusDirectory((DriverRef)self);
    de.name = "null";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.driver = (HandlerRef)self;
    de.arg = 0;

    return Driver_Publish(self, &de);
}

errno_t NullDriver_read(NullDriverRef _Nonnull self, IOChannelRef _Nonnull ioc, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    // Always return EOF
    *nOutBytesRead = 0;
    return EOK;
}

errno_t NullDriver_write(NullDriverRef _Nonnull self, IOChannelRef _Nonnull ioc, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    // Ignore output
    return EOK;
}


class_func_defs(NullDriver, PseudoDriver,
override_func_def(onStart, NullDriver, Driver)
override_func_def(read, NullDriver, Handler)
override_func_def(write, NullDriver, Handler)
);
