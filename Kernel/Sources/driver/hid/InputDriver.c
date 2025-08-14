//
//  InputDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "InputDriver.h"
#include <kpi/fcntl.h>


// Returns information about the disk drive and the media loaded into the
// drive.
void InputDriver_getInfo(InputDriverRef _Nonnull _Locked self, InputInfo* _Nonnull pOutInfo)
{
    pOutInfo->inputType = InputDriver_GetInputType(self);
}

InputType InputDriver_getInputType(InputDriverRef _Nonnull self)
{
    return kInputType_None;
}


//
// MARK: -
// HID Manager API
//

void InputDriver_getReport(InputDriverRef _Nonnull self, HIDReport* _Nonnull report)
{
    report->type = kHIDReportType_Null;
}

errno_t InputDriver_setReportTarget(InputDriverRef _Nonnull self, vcpu_t _Nullable vp, int signo)
{
    return ENOTSUP;
}


//
// MARK: -
// I/O Channel API
//

errno_t InputDriver_ioctl(InputDriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kInputCommand_GetInfo: {
            InputInfo* info = va_arg(ap, InputInfo*);
            
            InputDriver_GetInfo(self, info);
            return EOK;
        }

        default:
            return super_n(ioctl, Handler, InputDriver, self, pChannel, cmd, ap);
    }
}


class_func_defs(InputDriver, Driver,
func_def(getInfo, InputDriver)
func_def(getInputType, InputDriver)
func_def(getReport, InputDriver)
func_def(setReportTarget, InputDriver)
override_func_def(ioctl, InputDriver, Handler)
);
