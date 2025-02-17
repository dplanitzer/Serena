//
//  LogDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "LogDriver.h"
#include <log/Log.h>

final_class_ivars(LogDriver, Driver,
);


errno_t LogDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(LogDriver), kDriver_Exclusive, NULL, pOutSelf);
}

errno_t LogDriver_onStart(DriverRef _Nonnull _Locked self)
{
    return Driver_Publish(self, "klog", kRootUserId, kRootGroupId, FilePermissions_MakeFromOctal(0440), 0);
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
