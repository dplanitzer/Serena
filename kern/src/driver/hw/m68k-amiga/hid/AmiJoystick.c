//
//  AmiJoystick.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "AmiJoystick.h"
#include <hal/hw/m68k-amiga/chipset.h>
#include <hal/hw/m68k-amiga/cia8520.h>


final_class_ivars(AmiJoystick, IOHIDDevice,
    uint16_t    right_button_mask;
    uint8_t     fire_button_mask;
    int8_t      port;
);

IOCATS_DEF(g_cats, IOHID_DIGITAL_JOYSTICK);


errno_t AmiJoystick_Create(int port, IODriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AmiJoystickRef self;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(IODriver_Create(class(AmiJoystick), g_cats, (IODriverRef*)&self));
    self->port = (int8_t)port;
    
catch:
    *pOutSelf = (IODriverRef)self;
    return err;
}

errno_t AmiJoystick_start(AmiJoystickRef _Nonnull self)
{
    // Switch bit 7 and 6 to input
    hw_cia_a->ddra &= 0x3f;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    hw_chips->potgo &= 0x0f00;


    if (self->port == 0) {
        self->right_button_mask = POTGORF_DATLY;
        self->fire_button_mask = CIAA_PRAF_FIR0;
    }
    else {
        self->right_button_mask = POTGORF_DATRY;
        self->fire_button_mask = CIAA_PRAF_FIR1;
    }

    return EOK;
}

void AmiJoystick_getReport(AmiJoystickRef _Nonnull self, IOHIDReport* _Nonnull report)
{
    register const uint16_t joydat = (self->port == 0) ? hw_chips->joy0dat : hw_chips->joy1dat;
    int16_t x, y;
    uint8_t buttons = 0;

    
    // Left fire button
    if ((hw_cia_a->pra & self->fire_button_mask) == 0) {
        buttons |= 0x01;
    }
    
    
    // Right fire button
    if ((hw_chips->potinp & self->right_button_mask) == 0) {
        buttons |= 0x02;
    }

    
    // X axis
    if ((joydat & (1 << 1)) != 0) {
        x = INT16_MAX;  // right
    } else if ((joydat & (1 << 9)) != 0) {
        x = INT16_MIN;  // left
    }
    
    
    // Y axis
    register uint16_t joydat_xor = joydat ^ (joydat >> 1);
    if ((joydat & (1 << 0)) != 0) {
        y = INT16_MAX;  // down
    } else if ((joydat & (1 << 8)) != 0) {
        y = INT16_MIN;  // up
    }


    report->type = kIOHIDReportType_GamePad;
    report->data.joy.x = x;
    report->data.joy.y = y;
    report->data.joy.buttons = buttons;
}


class_func_defs(AmiJoystick, IOHIDDevice,
override_func_def(start, AmiJoystick, IODriver)
override_func_def(getReport, AmiJoystick, IOHIDDevice)
);
