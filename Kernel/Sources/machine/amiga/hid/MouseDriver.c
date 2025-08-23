//
//  MouseDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "MouseDriver.h"
#include <machine/amiga/chipset.h>


final_class_ivars(MouseDriver, InputDriver,
    volatile uint16_t* _Nonnull reg_joydat;
    volatile uint16_t* _Nonnull reg_potgor;
    volatile uint8_t* _Nonnull  reg_ciaa_pra;
    int16_t                     old_hcount;
    int16_t                     old_vcount;
    uint16_t                    right_button_mask;
    uint16_t                    middle_button_mask;
    uint8_t                     left_button_mask;
    int8_t                      port;
);

IOCATS_DEF(g_cats, IOHID_MOUSE);


errno_t MouseDriver_Create(int port, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    MouseDriverRef self;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(class(MouseDriver), kDriver_Exclusive, g_cats, (DriverRef*)&self));

    CHIPSET_BASE_DECL(cp);
    CIAA_BASE_DECL(ciaa);

    self->reg_joydat = (port == 0) ? CHIPSET_REG_16(cp, JOY0DAT) : CHIPSET_REG_16(cp, JOY1DAT);
    self->reg_potgor = CHIPSET_REG_16(cp, POTGOR);
    self->reg_ciaa_pra = CIA_REG_8(ciaa, 0);
    self->right_button_mask = (port == 0) ? POTGORF_DATLY : POTGORF_DATRY;
    self->middle_button_mask = (port == 0) ? POTGORF_DATLX : POTGORF_DATRX;
    self->left_button_mask = (port == 0) ? CIAA_PRAF_FIR0 : CIAA_PRAF_FIR1;
    self->port = (int8_t)port;

catch:
    *pOutSelf = (DriverRef)self;
    return err;
}

errno_t MouseDriver_onStart(MouseDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    char name[7];

    name[0] = 'm';
    name[1] = 'o';
    name[2] = 'u';
    name[3] = 's';
    name[4] = 'e';
    name[5] = '0' + self->port;
    name[6] = '\0';

    DriverEntry de;
    de.dirId = Driver_GetBusDirectory((DriverRef)self);
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0444);
    de.arg = 0;

    err = Driver_Publish(self, &de);
    if (err == EOK) {
        CHIPSET_BASE_DECL(cp);
        CIAA_BASE_DECL(ciaa);

        // Switch CIA PRA bit 7 and 6 to input for the left mouse button
        *CIA_REG_8(ciaa, CIA_DDRA) = *CIA_REG_8(ciaa, CIA_DDRA) & 0x3f;
    
        // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
        *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;
    }
    return err;
}

void MouseDriver_getReport(MouseDriverRef _Nonnull self, HIDReport* _Nonnull report)
{
    register uint16_t new_state = *(self->reg_joydat);
    
    // X delta
    register int16_t new_x = (int16_t)(new_state & 0x00ff);
    register int16_t dx = new_x - self->old_hcount;
    self->old_hcount = new_x;
    
    if (dx < -127) {
        // underflow
        dx = -255 - dx;
        if (dx < 0) dx = 0;
    } else if (dx > 127) {
        dx = 255 - dx;
        if (dx >= 0) dx = 0;
    }
    
    
    // Y delta
    register int16_t new_y = (int16_t)((new_state & 0xff00) >> 8);
    register int16_t dy = new_y - self->old_vcount;
    self->old_vcount = new_y;
    
    if (dy < -127) {
        // underflow
        dy = -255 - dy;
        if (dy < 0) dy = 0;
    } else if (dy > 127) {
        dy = 255 - dy;
        if (dy >= 0) dy = 0;
    }

    
    // Left mouse button
    register uint32_t buttons = 0;
    register uint8_t pra = *(self->reg_ciaa_pra);
    if ((pra & self->left_button_mask) == 0) {
        buttons |= 0x01;
    }
    
    
    // Right mouse button
    register uint16_t potgor = *(self->reg_potgor);
    if ((potgor & self->right_button_mask) == 0) {
        buttons |= 0x02;
    }

    
    // Middle mouse button
    if ((potgor & self->middle_button_mask) == 0) {
        buttons |= 0x04;
    }


    report->type = kHIDReportType_Mouse;
    report->data.mouse.dx = dx;
    report->data.mouse.dy = dy;
    report->data.mouse.buttons = buttons;
}


class_func_defs(MouseDriver, InputDriver,
override_func_def(onStart, MouseDriver, Driver)
override_func_def(getReport, MouseDriver, InputDriver)
);
