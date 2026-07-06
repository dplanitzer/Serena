//
//  AmiMouse.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "AmiMouse.h"
#include <hal/hw/m68k-amiga/chipset.h>
#include <hal/hw/m68k-amiga/cia8520.h>


final_class_ivars(AmiMouse, IOHIDDevice,
    int16_t     old_hcount;
    int16_t     old_vcount;
    uint16_t    right_button_mask;
    uint16_t    middle_button_mask;
    uint8_t     left_button_mask;
    int8_t      port;
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
    self->port = (int8_t)port;

catch:
    *pOutSelf = (IODriverRef)self;
    return err;
}

errno_t AmiMouse_start(AmiMouseRef _Nonnull self)
{
    // Switch CIA PRA bit 7 and 6 to input for the left mouse button
    hw_cia_a->ddra &= 0x3f;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    hw_chips->potgo &= 0x0f00;


    if (self->port == 0) {
        self->right_button_mask = POTGORF_DATLY;
        self->middle_button_mask = POTGORF_DATLX;
        self->left_button_mask = CIAA_PRAF_FIR0;
    }
    else {
        self->right_button_mask = POTGORF_DATRY;
        self->middle_button_mask = POTGORF_DATRX;
        self->left_button_mask = CIAA_PRAF_FIR1;
    }

    return EOK;
}

// Based on <https://www.markwrobel.dk/post/amiga-machine-code-letter11/>
void AmiMouse_getReport(AmiMouseRef _Nonnull self, IOHIDReport* _Nonnull report)
{
    register const uint16_t new_state = (self->port == 0) ? hw_chips->joy0dat : hw_chips->joy1dat;
    
    // X delta
    register const int16_t old_x = self->old_hcount;
    register const int16_t new_x = (int16_t)(new_state & 0x00ff);
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
    register const int16_t old_y = self->old_vcount;
    register const int16_t new_y = (int16_t)((new_state & 0xff00) >> 8);
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
    if ((hw_cia_a->pra & self->left_button_mask) == 0) {
        buttons |= 0x01;
    }
    
    
    // Right mouse button
    register const uint16_t potinp = hw_chips->potinp;
    if ((potinp & self->right_button_mask) == 0) {
        buttons |= 0x02;
    }

    
    // Middle mouse button
    if ((potinp & self->middle_button_mask) == 0) {
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
