//
//  AmiGPBus.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "AmiGPBus.h"
#include "AmiJoystick.h"
#include "AmiLightPen.h"
#include "AmiMouse.h"
#include "AmiPaddle.h"

errno_t AmiGPBus_createPortDriver(IOGPBusRef _Nonnull self, int port, int type, IODriverRef _Nullable * _Nonnull pOutDriver)
{
    switch (type) {
        case HID_PORT_MOUSE:
            return AmiMouse_Create(port, pOutDriver);

        case HID_PORT_LIGHT_PEN:
            return AmiLightPen_Create(port, pOutDriver);

        case HID_PORT_PADDLE:
            return AmiPaddle_Create(port, pOutDriver);

        case HID_PORT_JOYSTICK:
            return AmiJoystick_Create(port, pOutDriver);

        default:
            return EINVAL;
    }
}


class_func_defs(AmiGPBus, IOGPBus,
override_func_def(createPortDriver, AmiGPBus, IOGPBus)
);
