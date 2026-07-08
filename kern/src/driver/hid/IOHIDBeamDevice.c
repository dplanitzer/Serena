//
//  IOHIDBeamDevice.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IOHIDBeamDevice.h"

bool IOHIDBeamDevice_getBeamPosition(IOHIDBeamDeviceRef _Nonnull self, int16_t* _Nonnull x, int16_t* _Nonnull y)
{
    *x = 0;
    *y = 0;

    return false;
}


class_func_defs(IOHIDBeamDevice, IODriver,
func_def(getBeamPosition, IOHIDBeamDevice)
);
