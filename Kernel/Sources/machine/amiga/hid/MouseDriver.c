//
//  MouseDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "MouseDriver.h"
#include <driver/hid/HIDManager.h>
#include <machine/InterruptController.h>
#include <machine/amiga/chipset.h>
#include <machine/irq.h>


final_class_ivars(MouseDriver, InputDriver,
    InterruptHandlerID          irqHandler;
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

extern void MouseDriver_OnInterrupt(MouseDriverRef _Nonnull self);


errno_t MouseDriver_Create(CatalogId parentDirId, int port, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    CIAA_BASE_DECL(ciaa);
    MouseDriverRef self = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(class(MouseDriver), kDriver_Exclusive, parentDirId, (DriverRef*)&self));
    
    self->reg_joydat = (port == 0) ? CHIPSET_REG_16(cp, JOY0DAT) : CHIPSET_REG_16(cp, JOY1DAT);
    self->reg_potgor = CHIPSET_REG_16(cp, POTGOR);
    self->reg_ciaa_pra = CIA_REG_8(ciaa, 0);
    self->right_button_mask = (port == 0) ? POTGORF_DATLY : POTGORF_DATRY;
    self->middle_button_mask = (port == 0) ? POTGORF_DATLX : POTGORF_DATRX;
    self->left_button_mask = (port == 0) ? CIAA_PRAF_FIR0 : CIAA_PRAF_FIR1;
    self->port = (int8_t)port;

    // Switch CIA PRA bit 7 and 6 to input for the left mouse button
    *CIA_REG_8(ciaa, CIA_DDRA) = *CIA_REG_8(ciaa, CIA_DDRA) & 0x3f;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;

    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 2,
                                                      (InterruptHandler_Closure)MouseDriver_OnInterrupt,
                                                      self,
                                                      &self->irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, self->irqHandler, true);

    *pOutSelf = (DriverRef)self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void MouseDriver_deinit(MouseDriverRef _Nonnull self)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->irqHandler));
}

errno_t MouseDriver_onStart(MouseDriverRef _Nonnull _Locked self)
{
    char name[7];

    name[0] = 'm';
    name[1] = 'o';
    name[2] = 'u';
    name[3] = 's';
    name[4] = 'e';
    name[5] = '0' + self->port;
    name[6] = '\0';

    DriverEntry de;
    de.dirId = Driver_GetParentDirectoryId(self);
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0444);
    de.category = 0;
    de.driver = (HandlerRef)self;
    de.arg = 0;

    return Driver_Publish(self, &de);
}

void MouseDriver_onStop(DriverRef _Nonnull _Locked self)
{
    Driver_Unpublish(self);
}

InputType MouseDriver_getInputType(MouseDriverRef _Nonnull self)
{
    return kInputType_Mouse;
}

void MouseDriver_OnInterrupt(MouseDriverRef _Nonnull self)
{
    register uint16_t new_state = *(self->reg_joydat);
    
    // X delta
    register int16_t new_x = (int16_t)(new_state & 0x00ff);
    register int16_t xDelta = new_x - self->old_hcount;
    self->old_hcount = new_x;
    
    if (xDelta < -127) {
        // underflow
        xDelta = -255 - xDelta;
        if (xDelta < 0) xDelta = 0;
    } else if (xDelta > 127) {
        xDelta = 255 - xDelta;
        if (xDelta >= 0) xDelta = 0;
    }
    
    
    // Y delta
    register int16_t new_y = (int16_t)((new_state & 0xff00) >> 8);
    register int16_t yDelta = new_y - self->old_vcount;
    self->old_vcount = new_y;
    
    if (yDelta < -127) {
        // underflow
        yDelta = -255 - yDelta;
        if (yDelta < 0) yDelta = 0;
    } else if (yDelta > 127) {
        yDelta = 255 - yDelta;
        if (yDelta >= 0) yDelta = 0;
    }

    
    // Left mouse button
    register uint32_t buttonsDown = 0;
    register uint8_t pra = *(self->reg_ciaa_pra);
    if ((pra & self->left_button_mask) == 0) {
        buttonsDown |= 0x01;
    }
    
    
    // Right mouse button
    register uint16_t potgor = *(self->reg_potgor);
    if ((potgor & self->right_button_mask) == 0) {
        buttonsDown |= 0x02;
    }

    
    // Middle mouse button
    if ((potgor & self->middle_button_mask) == 0) {
        buttonsDown |= 0x04;
    }


    HIDManager_ReportMouseDeviceChange(gHIDManager, xDelta, yDelta, buttonsDown);
}


class_func_defs(MouseDriver, InputDriver,
override_func_def(deinit, MouseDriver, Object)
override_func_def(onStart, MouseDriver, Driver)
override_func_def(onStop, MouseDriver, Driver)
override_func_def(getInputType, MouseDriver, InputDriver)
);
