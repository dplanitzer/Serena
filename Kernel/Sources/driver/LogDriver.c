//
//  LogDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "LogDriver.h"
#include <driver/DriverManager.h>
#include <log/Log.h>

final_class_ivars(LogDriver, Driver,
);


errno_t LogDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(LogDriver), kDriver_Exclusive, kCatalogId_None, pOutSelf);
}

errno_t LogDriver_onStart(DriverRef _Nonnull _Locked self)
{
    DriverEntry1 de;
    de.dirId = Driver_GetParentDirectoryId(self);
    de.name = "klog";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0440);
    de.arg = 0;

    return DriverManager_Publish(gDriverManager, self, &de);
}

errno_t LogDriver_read(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    *nOutBytesRead = log_read(pBuffer, nBytesToRead);
    return EOK;
}

errno_t LogDriver_write(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    log_write(pBuffer, nBytesToWrite);
    *nOutBytesWritten = nBytesToWrite;
    return EOK;
}


class_func_defs(LogDriver, Driver,
override_func_def(onStart, LogDriver, Driver)
override_func_def(read, LogDriver, Driver)
override_func_def(write, LogDriver, Driver)
);
