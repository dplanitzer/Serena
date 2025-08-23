//
//  LogDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "LogDriver.h"
#include <log/Log.h>


final_class_ivars(LogDriver, PseudoDriver,
);


errno_t LogDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return PseudoDriver_Create(class(LogDriver), 0, pOutSelf);
}

errno_t LogDriver_onStart(LogDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    DriverEntry de;

    de.name = "klog";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0440);
    de.arg = 0;

    return Driver_Publish(self, &de);
}

errno_t LogDriver_read(LogDriverRef _Nonnull self, IOChannelRef _Nonnull ioc, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    *nOutBytesRead = log_read(buf, nBytesToRead);
    return EOK;
}

errno_t LogDriver_write(LogDriverRef _Nonnull self, IOChannelRef _Nonnull ioc, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    log_write(buf, nBytesToWrite);
    *nOutBytesWritten = nBytesToWrite;
    return EOK;
}


class_func_defs(LogDriver, PseudoDriver,
override_func_def(onStart, LogDriver, Driver)
override_func_def(read, LogDriver, Driver)
override_func_def(write, LogDriver, Driver)
);
