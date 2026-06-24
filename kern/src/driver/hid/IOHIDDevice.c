//
//  IOHIDDevice.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "IOHIDDevice.h"


//
// MARK: -
// HID Manager API
//

void IOHIDDevice_getReport(IOHIDDeviceRef _Nonnull self, IOHIDReport* _Nonnull report)
{
    report->type = kIOHIDReportType_Null;
}

errno_t IOHIDDevice_setReportTarget(IOHIDDeviceRef _Nonnull self, vcpu_t _Nullable vp, int signo)
{
    return ENOTSUP;
}

class_func_defs(IOHIDDevice, Driver,
func_def(getReport, IOHIDDevice)
func_def(setReportTarget, IOHIDDevice)
);
