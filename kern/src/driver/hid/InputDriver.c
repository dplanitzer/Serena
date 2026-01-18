//
//  InputDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "InputDriver.h"


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

class_func_defs(InputDriver, Driver,
func_def(getReport, InputDriver)
func_def(setReportTarget, InputDriver)
);
