//
//  NullDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "NullDriver.h"
#include <driver/DriverManager.h>

final_class_ivars(NullDriver, Driver,
);


errno_t NullDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(NullDriver), 0, NULL, kCatalogId_None, pOutSelf);
}

errno_t NullDriver_onStart(DriverRef _Nonnull _Locked self)
{
    DriverEntry1 de;
    de.dirId = Driver_GetParentDirectoryId(self);
    de.name = "null";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.arg = 0;

    return DriverManager_Publish(gDriverManager, self, &de);
}

errno_t NullDriver_read(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    // Always return EOF
    *nOutBytesRead = 0;
    return EOK;
}

errno_t NullDriver_write(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    // Ignore output
    return EOK;
}


class_func_defs(NullDriver, Driver,
override_func_def(onStart, NullDriver, Driver)
override_func_def(read, NullDriver, Driver)
override_func_def(write, NullDriver, Driver)
);
