//
//  AmiMouse.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "AmiMouse.h"
#include <hal/hw/m68k-amiga/chipset.h>


final_class_ivars(AmiMouse, IOHIDDevice,
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


errno_t AmiMouse_Create(int port, IODriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AmiMouseRef self;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(IODriver_Create(class(AmiMouse), g_cats, (IODriverRef*)&self));

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
    *pOutSelf = (IODriverRef)self;
    return err;
}

errno_t AmiMouse_start(AmiMouseRef _Nonnull self)
{
    CHIPSET_BASE_DECL(cp);
    CIAA_BASE_DECL(ciaa);

    // Switch CIA PRA bit 7 and 6 to input for the left mouse button
    *CIA_REG_8(ciaa, CIA_DDRA) = *CIA_REG_8(ciaa, CIA_DDRA) & 0x3f;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;

    return EOK;
}

// Based on <https://www.markwrobel.dk/post/amiga-machine-code-letter11/>
void AmiMouse_getReport(AmiMouseRef _Nonnull self, IOHIDReport* _Nonnull report)
{
    register uint16_t new_state = *(self->reg_joydat);
    
    // X delta
    register int16_t old_x = self->old_hcount;
    register int16_t new_x = (int16_t)(new_state & 0x00ff);
    register int16_t dx = new_x - old_x;
    self->old_hcount = new_x;
    
    if (dx < -128) {
        dx += 256;
    }
    else if (dx > 127) {
        dx -= 256;
    }
    else if (dx < 0) {
        if (new_x > old_x) {
            dx = -dx;
        }
    }
    else if (new_x < old_x) {
        dx = -dx;
    }
    
    
    // Y delta
    register int16_t old_y = self->old_vcount;
    register int16_t new_y = (int16_t)((new_state & 0xff00) >> 8);
    register int16_t dy = new_y - old_y;
    self->old_vcount = new_y;

    if (dy < -128) {
        dy += 256;
    }
    else if (dy > 127) {
        dy -= 256;
    }
    else if (dy < 0) {
        if (new_y > old_y) {
            dy = -dy;
        }
    }
    else if (new_y < old_y) {
        dy = -dy;
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


    report->type = kIOHIDReportType_Mouse;
    report->data.mouse.dx = dx;
    report->data.mouse.dy = dy;
    report->data.mouse.buttons = buttons;
}


class_func_defs(AmiMouse, IOHIDDevice,
override_func_def(start, AmiMouse, IODriver)
override_func_def(getReport, AmiMouse, IOHIDDevice)
);
