//
//  LogDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "LogDriver.h"
#include <kern/log.h>

IOCATS_DEF(g_cats, IOSRV_KLOG);


final_class_ivars(LogDriver, Driver,
);


errno_t LogDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_CreateRoot(class(LogDriver), 0, g_cats, pOutSelf);
}

errno_t LogDriver_onStart(LogDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    DriverEntry de;

    de.name = "klog";
    de.uid = UID_ROOT;
    de.gid = GID_ROOT;
    de.perms = fs_perms_from_octal(0440);
    de.arg = 0;

    return Driver_Publish((DriverRef)self, &de);
}

errno_t LogDriver_read(LogDriverRef _Nonnull self, fd_flags_t flags, off_t* _Nonnull pOffset, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    *nOutBytesRead = log_read(buf, nBytesToRead);
    return EOK;
}

errno_t LogDriver_write(LogDriverRef _Nonnull self, fd_flags_t flags, off_t* _Nonnull pOffset, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    log_write(buf, nBytesToWrite);
    *nOutBytesWritten = nBytesToWrite;
    return EOK;
}


class_func_defs(LogDriver, Driver,
override_func_def(onStart, LogDriver, Driver)
override_func_def(read, LogDriver, Driver)
override_func_def(write, LogDriver, Driver)
);
