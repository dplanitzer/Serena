//
//  JoystickDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "JoystickDriver.h"
#include <hal/InterruptController.h>



////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Digital Joystick Driver
////////////////////////////////////////////////////////////////////////////////

final_class_ivars(DigitalJoystickDriver, InputDriver,
    EventDriverRef _Nonnull eventDriver;
    InterruptHandlerID      irqHandler;
    volatile uint16_t*      reg_joydat;
    volatile uint16_t*      reg_potgor;
    volatile uint8_t*       reg_ciaa_pra;
    uint16_t                right_button_mask;
    uint8_t                 fire_button_mask;
    int8_t                  port;
);

extern void DigitalJoystickDriver_OnInterrupt(DigitalJoystickDriverRef _Nonnull self);


errno_t DigitalJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, DigitalJoystickDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    CIAA_BASE_DECL(ciaa);
    DigitalJoystickDriverRef self = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(DigitalJoystickDriver, 0, &self));
    
    self->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    self->reg_joydat = (port == 0) ? CHIPSET_REG_16(cp, JOY0DAT) : CHIPSET_REG_16(cp, JOY1DAT);
    self->reg_potgor = CHIPSET_REG_16(cp, POTGOR);
    self->reg_ciaa_pra = CIA_REG_8(ciaa, 0);
    self->right_button_mask = (port == 0) ? POTGORF_DATLY : POTGORF_DATRY;
    self->fire_button_mask = (port == 0) ? CIAA_PRAF_FIR0 : CIAA_PRAF_FIR1;
    self->port = (int8_t)port;
    
    // Switch bit 7 and 6 to input
    *CIA_REG_8(ciaa, CIA_DDRA) = *CIA_REG_8(ciaa, CIA_DDRA) & 0x3f;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;

    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 1,
                                                      (InterruptHandler_Closure)DigitalJoystickDriver_OnInterrupt,
                                                      self,
                                                      &self->irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, self->irqHandler, true);

    *pOutSelf = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void DigitalJoystickDriver_deinit(DigitalJoystickDriverRef _Nonnull self)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->irqHandler));
    
    Object_Release(self->eventDriver);
    self->eventDriver = NULL;
}

InputType DigitalJoystickDriver_getInputType(DigitalJoystickDriverRef _Nonnull self)
{
    return kInputType_DigitalJoystick;
}

void DigitalJoystickDriver_OnInterrupt(DigitalJoystickDriverRef _Nonnull self)
{
    register uint8_t pra = *(self->reg_ciaa_pra);
    register uint16_t joydat = *(self->reg_joydat);
    int16_t xAbs, yAbs;
    uint32_t buttonsDown = 0;

    
    // Left fire button
    if ((pra & self->fire_button_mask) == 0) {
        buttonsDown |= 0x01;
    }
    
    
    // Right fire button
    register uint16_t potgor = *(self->reg_potgor);
    if ((potgor & self->right_button_mask) == 0) {
        buttonsDown |= 0x02;
    }

    
    // X axis
    if ((joydat & (1 << 1)) != 0) {
        xAbs = INT16_MAX;  // right
    } else if ((joydat & (1 << 9)) != 0) {
        xAbs = INT16_MIN;  // left
    }
    
    
    // Y axis
    register uint16_t joydat_xor = joydat ^ (joydat >> 1);
    if ((joydat & (1 << 0)) != 0) {
        yAbs = INT16_MAX;  // down
    } else if ((joydat & (1 << 8)) != 0) {
        yAbs = INT16_MIN;  // up
    }


    EventDriver_ReportJoystickDeviceChange(self->eventDriver, self->port, xAbs, yAbs, buttonsDown);
}


class_func_defs(DigitalJoystickDriver, InputDriver,
override_func_def(deinit, DigitalJoystickDriver, Object)
override_func_def(getInputType, DigitalJoystickDriver, InputDriver)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Analog Joystick (Paddles) Driver
////////////////////////////////////////////////////////////////////////////////

final_class_ivars(AnalogJoystickDriver, InputDriver,
    EventDriverRef _Nonnull eventDriver;
    InterruptHandlerID      irqHandler;
    volatile uint16_t*      reg_joydat;
    volatile uint16_t*      reg_potdat;
    volatile uint16_t*      reg_potgo;
    int16_t                 smoothedX;
    int16_t                 smoothedY;
    int16_t                 sumX;
    int16_t                 sumY;
    int8_t                  sampleCount;    // How many samples to average to produce a smoothed value
    int8_t                  sampleIndex;    // Current sample in the range 0..<sampleCount
    int8_t                  port;
);

extern void AnalogJoystickDriver_OnInterrupt(AnalogJoystickDriverRef _Nonnull self);


errno_t AnalogJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, AnalogJoystickDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    AnalogJoystickDriverRef self = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(AnalogJoystickDriver, 0, &self));
    
    self->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    self->reg_joydat = (port == 0) ? CHIPSET_REG_16(cp, JOY0DAT) : CHIPSET_REG_16(cp, JOY1DAT);
    self->reg_potdat = (port == 0) ? CHIPSET_REG_16(cp, POT0DAT) : CHIPSET_REG_16(cp, POT1DAT);
    self->reg_potgo = CHIPSET_REG_16(cp, POTGO);
    self->port = (int8_t)port;
    self->sampleCount = 4;
    self->sampleIndex = 0;
    self->sumX = 0;
    self->sumY = 0;
    self->smoothedX = 0;
    self->smoothedY = 0;
    
    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 1,
                                                      (InterruptHandler_Closure)AnalogJoystickDriver_OnInterrupt,
                                                      self,
                                                      &self->irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, self->irqHandler, true);

    *pOutSelf = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void AnalogJoystickDriver_deinit(AnalogJoystickDriverRef _Nonnull self)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->irqHandler));
    
    Object_Release(self->eventDriver);
    self->eventDriver = NULL;
}

InputType AnalogJoystickDriver_getInputType(AnalogJoystickDriverRef _Nonnull self)
{
    return kInputType_AnalogJoystick;
}

void AnalogJoystickDriver_OnInterrupt(AnalogJoystickDriverRef _Nonnull self)
{
    register uint16_t potdat = *(self->reg_potdat);
    register uint16_t joydat = *(self->reg_joydat);

    // Return the smoothed value
    const int16_t xAbs = self->smoothedX;
    const int16_t yAbs = self->smoothedY;
    uint32_t buttonsDown = 0;
    
    
    // Sum up to 'sampleCount' samples and then compute the smoothed out value
    // as the average of 'sampleCount' samples.
    if (self->sampleIndex == self->sampleCount) {
        self->smoothedX = (self->sumX / self->sampleCount) << 8;
        self->smoothedY = (self->sumY / self->sampleCount) << 8;
        self->sampleIndex = 0;
        self->sumX = 0;
        self->sumY = 0;
    } else {
        self->sampleIndex++;
        
        // X axis
        const int16_t xval = (int16_t)(potdat & 0x00ff);
        self->sumX += (xval - 128);
        
        
        // Y axis
        const int16_t yval = (int16_t)((potdat >> 8) & 0x00ff);
        self->sumY += (yval - 128);
    }

    
    // Left fire button
    if ((joydat & (1 << 9)) != 0) {
        buttonsDown |= 0x01;
    }
    
    
    // Right fire button
    if ((joydat & (1 << 1)) != 0) {
        buttonsDown |= 0x02;
    }
    
    
    // Restart the counter for the next frame
    *(self->reg_potgo) = 0x0001;
    
    
    EventDriver_ReportJoystickDeviceChange(self->eventDriver, self->port, xAbs, yAbs, buttonsDown);
}


class_func_defs(AnalogJoystickDriver, InputDriver,
override_func_def(deinit, AnalogJoystickDriver, Object)
override_func_def(getInputType, AnalogJoystickDriver, InputDriver)
);
