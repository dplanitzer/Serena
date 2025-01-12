//
//  InputDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "InputDriver.h"
#include <driver/DriverChannel.h>


// Returns information about the disk drive and the media loaded into the
// drive.
void InputDriver_getInfo(InputDriverRef _Nonnull _Locked self, InputInfo* _Nonnull pOutInfo)
{
    pOutInfo->inputType = InputDriver_GetInputType(self);
}

errno_t InputDriver_GetInfo(InputDriverRef _Nonnull self, InputInfo* pOutInfo)
{
    decl_try_err();

    Driver_Lock(self);
    if (Driver_IsActive(self)) {
        invoke_n(getInfo, InputDriver, self, pOutInfo);
    }
    else {
        err = ENODEV;
    }
    Driver_Unlock(self);

    return err;
}


InputType InputDriver_getInputType(InputDriverRef _Nonnull self)
{
    return kInputType_None;
}


//
// MARK: -
// I/O Channel API
//

errno_t InputDriver_ioctl(InputDriverRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kInputCommand_GetInfo:
            return InputDriver_GetInfo(self, va_arg(ap, InputInfo*));
            
        default:
            return super_n(ioctl, Driver, InputDriver, self, cmd, ap);
    }
}


class_func_defs(InputDriver, Driver,
func_def(getInfo, InputDriver)
func_def(getInputType, InputDriver)
override_func_def(ioctl, InputDriver, Driver)
);
