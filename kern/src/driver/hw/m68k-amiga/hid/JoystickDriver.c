//
//  JoystickDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "JoystickDriver.h"
#include <hal/hw/m68k-amiga/chipset.h>


final_class_ivars(JoystickDriver, InputDriver,
    volatile uint16_t* _Nonnull reg_joydat;
    volatile uint16_t* _Nonnull reg_potgor;
    volatile uint8_t* _Nonnull  reg_ciaa_pra;
    uint16_t                    right_button_mask;
    uint8_t                     fire_button_mask;
    int8_t                      port;
);

IOCATS_DEF(g_cats, IOHID_DIGITAL_JOYSTICK);


errno_t JoystickDriver_Create(int port, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    JoystickDriverRef self;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(class(JoystickDriver), kDriver_Exclusive, g_cats, (DriverRef*)&self));
    
    CHIPSET_BASE_DECL(cp);
    CIAA_BASE_DECL(ciaa);

    self->reg_joydat = (port == 0) ? CHIPSET_REG_16(cp, JOY0DAT) : CHIPSET_REG_16(cp, JOY1DAT);
    self->reg_potgor = CHIPSET_REG_16(cp, POTGOR);
    self->reg_ciaa_pra = CIA_REG_8(ciaa, 0);
    self->right_button_mask = (port == 0) ? POTGORF_DATLY : POTGORF_DATRY;
    self->fire_button_mask = (port == 0) ? CIAA_PRAF_FIR0 : CIAA_PRAF_FIR1;
    self->port = (int8_t)port;
    
catch:
    *pOutSelf = (DriverRef)self;
    return err;
}

errno_t JoystickDriver_onStart(JoystickDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    char name[10];

    name[0] = 'j';
    name[1] = 'o';
    name[2] = 'y';
    name[3] = 's';
    name[4] = 't';
    name[5] = 'i';
    name[6] = 'c';
    name[7] = 'k';
    name[8] = '0' + self->port;
    name[9] = '\0';

    DriverEntry de;
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0444);
    de.arg = 0;

    err = Driver_Publish((DriverRef)self, &de);
    if (err == EOK) {
        CHIPSET_BASE_DECL(cp);
        CIAA_BASE_DECL(ciaa);

        // Switch bit 7 and 6 to input
        *CIA_REG_8(ciaa, CIA_DDRA) = *CIA_REG_8(ciaa, CIA_DDRA) & 0x3f;
    
        // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
        *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;
    }
    return err;
}

void JoystickDriver_getReport(JoystickDriverRef _Nonnull self, HIDReport* _Nonnull report)
{
    register uint8_t pra = *(self->reg_ciaa_pra);
    register uint16_t joydat = *(self->reg_joydat);
    int16_t x, y;
    uint8_t buttons = 0;

    
    // Left fire button
    if ((pra & self->fire_button_mask) == 0) {
        buttons |= 0x01;
    }
    
    
    // Right fire button
    register uint16_t potgor = *(self->reg_potgor);
    if ((potgor & self->right_button_mask) == 0) {
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


    report->type = kHIDReportType_Joystick;
    report->data.joy.x = x;
    report->data.joy.y = y;
    report->data.joy.buttons = buttons;
}


class_func_defs(JoystickDriver, InputDriver,
override_func_def(onStart, JoystickDriver, Driver)
override_func_def(getReport, JoystickDriver, InputDriver)
);
