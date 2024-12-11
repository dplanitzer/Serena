//
//  InputDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "InputDriver.h"
#include <driver/hid/HIDKeyRepeater.h>
#include <hal/InterruptController.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Keyboard Driver
////////////////////////////////////////////////////////////////////////////////

// Keycode -> USB HID keyscan codes
// See: <http://whdload.de/docs/en/rawkey.html>
// See: <http://www.quadibloc.com/comp/scan.htm>
static const uint8_t gUSBHIDKeycodes[128] = {
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x31, 0x00, 0x62, // $00 - $0f
    0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x00, 0x59, 0x5a, 0x5b, // $10 - $1f
    0x04, 0x16, 0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x00, 0x00, 0x5c, 0x5d, 0x5e, // $20 - $2f
    0x36, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0x00, 0x63, 0x5f, 0x60, 0x61, // $30 - $3f
    0x2c, 0x2a, 0x2b, 0x58, 0x28, 0x29, 0x4c, 0x00, 0x00, 0x00, 0x56, 0x00, 0x52, 0x51, 0x4f, 0x50, // $40 - $4f
    0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x53, 0x54, 0x55, 0x56, 0x57, 0x75, // $50 - $5f
    0xe1, 0xe5, 0x39, 0xe0, 0xe2, 0xe6, 0xe3, 0xe7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $60 - $6f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, // $70 - $7f
};


final_class_ivars(KeyboardDriver, Driver,
    const uint8_t* _Nonnull       keyCodeMap;
    EventDriverRef _Nonnull     eventDriver;
    HIDKeyRepeaterRef _Nonnull  keyRepeater;
    InterruptHandlerID          keyboardIrqHandler;
    InterruptHandlerID          vblIrqHandler;
);


extern void ksb_init(void);
extern int ksb_receive_key(void);
extern void ksb_acknowledge_key(void);
extern void KeyboardDriver_OnKeyboardInterrupt(KeyboardDriverRef _Nonnull self);
extern void KeyboardDriver_OnVblInterrupt(KeyboardDriverRef _Nonnull self);


errno_t KeyboardDriver_Create(EventDriverRef _Nonnull pEventDriver, KeyboardDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    KeyboardDriverRef self;
    
    try(Driver_Create(KeyboardDriver, kDriverModel_Sync, 0, &self));
    
    self->keyCodeMap = gUSBHIDKeycodes;
    self->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    try(HIDKeyRepeater_Create(pEventDriver, &self->keyRepeater));

    ksb_init();

    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_CIA_A_SP,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL,
                                                      (InterruptHandler_Closure)KeyboardDriver_OnKeyboardInterrupt,
                                                      self,
                                                      &self->keyboardIrqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, self->keyboardIrqHandler, true);

    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 1,
                                                      (InterruptHandler_Closure)KeyboardDriver_OnVblInterrupt,
                                                      self,
                                                      &self->vblIrqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, self->vblIrqHandler, true);

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void KeyboardDriver_deinit(KeyboardDriverRef _Nonnull self)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->keyboardIrqHandler));
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->vblIrqHandler));

    HIDKeyRepeater_Destroy(self->keyRepeater);
    self->keyRepeater = NULL;

    Object_Release(self->eventDriver);
    self->eventDriver = NULL;
}

void KeyboardDriver_GetKeyRepeatDelays(KeyboardDriverRef _Nonnull self, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay)
{
    const int irs = cpu_disable_irqs();
    HIDKeyRepeater_GetKeyRepeatDelays(self->keyRepeater, pInitialDelay, pRepeatDelay);
    cpu_restore_irqs(irs);
}

void KeyboardDriver_SetKeyRepeatDelays(KeyboardDriverRef _Nonnull self, TimeInterval initialDelay, TimeInterval repeatDelay)
{
    const int irs = cpu_disable_irqs();
    HIDKeyRepeater_SetKeyRepeatDelays(self->keyRepeater, initialDelay, repeatDelay);
    cpu_restore_irqs(irs);
}

void KeyboardDriver_OnKeyboardInterrupt(KeyboardDriverRef _Nonnull self)
{
    const uint8_t keyCode = ksb_receive_key();
    const HIDKeyState state = (keyCode & 0x80) ? kHIDKeyState_Up : kHIDKeyState_Down;
    const uint16_t code = (uint16_t)self->keyCodeMap[keyCode & 0x7f];

    if (code > 0) {
        EventDriver_ReportKeyboardDeviceChange(self->eventDriver, state, code);
        if (state == kHIDKeyState_Up) {
            HIDKeyRepeater_KeyUp(self->keyRepeater, code);
        } else {
            HIDKeyRepeater_KeyDown(self->keyRepeater, code);
        }
    }
    ksb_acknowledge_key();
}

void KeyboardDriver_OnVblInterrupt(KeyboardDriverRef _Nonnull self)
{
    // const int = cpu_disable_irqs();
    HIDKeyRepeater_Tick(self->keyRepeater);
    // cpu_restore_irqs(irs);
}


class_func_defs(KeyboardDriver, Driver,
override_func_def(deinit, KeyboardDriver, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Driver
////////////////////////////////////////////////////////////////////////////////

final_class_ivars(MouseDriver, Driver,
    EventDriverRef _Nonnull eventDriver;
    InterruptHandlerID      irqHandler;
    volatile uint16_t*        reg_joydat;
    volatile uint16_t*        reg_potgor;
    volatile uint8_t*         reg_ciaa_pra;
    int16_t                   old_hcount;
    int16_t                   old_vcount;
    uint16_t                  right_button_mask;
    uint16_t                  middle_button_mask;
    uint8_t                   left_button_mask;
    int8_t                    port;
);

extern void MouseDriver_OnInterrupt(MouseDriverRef _Nonnull self);


errno_t MouseDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, MouseDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    CIAA_BASE_DECL(ciaa);
    MouseDriverRef self = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(MouseDriver, kDriverModel_Sync, 0, &self));
    
    self->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
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

    *pOutSelf = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void MouseDriver_deinit(MouseDriverRef _Nonnull self)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->irqHandler));

    Object_Release(self->eventDriver);
    self->eventDriver = NULL;
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


    EventDriver_ReportMouseDeviceChange(self->eventDriver, xDelta, yDelta, buttonsDown);
}


class_func_defs(MouseDriver, Driver,
override_func_def(deinit, MouseDriver, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Digital Joystick Driver
////////////////////////////////////////////////////////////////////////////////

final_class_ivars(DigitalJoystickDriver, Driver,
    EventDriverRef _Nonnull eventDriver;
    InterruptHandlerID      irqHandler;
    volatile uint16_t*        reg_joydat;
    volatile uint16_t*        reg_potgor;
    volatile uint8_t*         reg_ciaa_pra;
    uint16_t                  right_button_mask;
    uint8_t                   fire_button_mask;
    int8_t                    port;
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
    
    try(Driver_Create(DigitalJoystickDriver, kDriverModel_Sync, 0, &self));
    
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


class_func_defs(DigitalJoystickDriver, Driver,
override_func_def(deinit, DigitalJoystickDriver, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Analog Joystick (Paddles) Driver
////////////////////////////////////////////////////////////////////////////////

final_class_ivars(AnalogJoystickDriver, Driver,
    EventDriverRef _Nonnull eventDriver;
    InterruptHandlerID      irqHandler;
    volatile uint16_t*        reg_joydat;
    volatile uint16_t*        reg_potdat;
    volatile uint16_t*        reg_potgo;
    int16_t                   smoothedX;
    int16_t                   smoothedY;
    int16_t                   sumX;
    int16_t                   sumY;
    int8_t                    sampleCount;    // How many samples to average to produce a smoothed value
    int8_t                    sampleIndex;    // Current sample in the range 0..<sampleCount
    int8_t                    port;
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
    
    try(Driver_Create(AnalogJoystickDriver, kDriverModel_Sync, 0, &self));
    
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


class_func_defs(AnalogJoystickDriver, Driver,
override_func_def(deinit, AnalogJoystickDriver, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Light Pen Driver
////////////////////////////////////////////////////////////////////////////////

final_class_ivars(LightPenDriver, Driver,
    EventDriverRef _Nonnull     eventDriver;
    GraphicsDriverRef _Nonnull  gdevice;
    InterruptHandlerID          irqHandler;
    volatile uint16_t*            reg_potgor;
    uint16_t                      right_button_mask;
    uint16_t                      middle_button_mask;
    int16_t                       smoothedX;
    int16_t                       smoothedY;
    bool                        hasSmoothedPosition;    // True if the light pen position is available (pen triggered the position latching hardware); false otherwise
    int16_t                       sumX;
    int16_t                       sumY;
    int8_t                        sampleCount;    // How many samples to average to produce a smoothed value
    int8_t                        sampleIndex;    // Current sample in the range 0..<sampleCount
    int8_t                        triggerCount;   // Number of times that the light pen has triggered in the 'sampleCount' interval
    int8_t                        port;
);

extern void LightPenDriver_OnInterrupt(LightPenDriverRef _Nonnull self);


errno_t LightPenDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, LightPenDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    LightPenDriverRef self = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(LightPenDriver, kDriverModel_Sync, 0, &self));
    
    self->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    self->gdevice = Object_RetainAs(EventDriver_GetGraphicsDriver(pEventDriver), GraphicsDriver);
    self->reg_potgor = CHIPSET_REG_16(cp, POTGOR);
    self->right_button_mask = (port == 0) ? POTGORF_DATLY : POTGORF_DATRY;
    self->middle_button_mask = (port == 0) ? POTGORF_DATLX : POTGORF_DATRX;
    self->smoothedX = 0;
    self->smoothedY = 0;
    self->sumX = 0;
    self->sumY = 0;
    self->hasSmoothedPosition = false;
    self->sampleCount = 4;
    self->sampleIndex = 0;
    self->triggerCount = 0;
    self->port = (int8_t)port;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;
    
    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 1,
                                                      (InterruptHandler_Closure)LightPenDriver_OnInterrupt,
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

static void LightPenDriver_deinit(LightPenDriverRef _Nonnull self)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->irqHandler));
    
    Object_Release(self->gdevice);
    self->gdevice = NULL;

    Object_Release(self->eventDriver);
    self->eventDriver = NULL;
}

void LightPenDriver_OnInterrupt(LightPenDriverRef _Nonnull self)
{
    // Return the smoothed value
    int16_t xAbs = self->smoothedX;
    int16_t yAbs = self->smoothedY;
    const bool hasPosition = self->hasSmoothedPosition;
    uint32_t buttonsDown = 0;

    
    // Sum up to 'sampleCount' samples and then compute the smoothed out value
    // as the average of 'sampleCount' samples.
    if (self->sampleIndex == self->sampleCount) {
        self->smoothedX = self->triggerCount ? (self->sumX / self->triggerCount) << 8 : 0;
        self->smoothedY = self->triggerCount ? (self->sumY / self->triggerCount) << 8 : 0;
        self->hasSmoothedPosition = self->triggerCount >= (self->sampleCount / 2);
        self->sampleIndex = 0;
        self->triggerCount = 0;
        self->sumX = 0;
        self->sumY = 0;
    } else {
        self->sampleIndex++;
    
        // Get the position
        int16_t xPos, yPos;
        
        if (GraphicsDriver_GetLightPenPosition(self->gdevice, &xPos, &yPos)) {
            self->triggerCount++;
            self->sumX += xPos;
            self->sumY += yPos;
        }
    }

    
    // Button #0
    register uint16_t potgor = *(self->reg_potgor);
    if ((potgor & self->right_button_mask) == 0) {
        buttonsDown |= 0x02;
    }
    
    
    // Button # 1
    if ((potgor & self->middle_button_mask) == 0) {
        buttonsDown |= 0x04;
    }
    

    EventDriver_ReportLightPenDeviceChange(self->eventDriver, xAbs, yAbs, hasPosition, buttonsDown);
}


class_func_defs(LightPenDriver, Driver,
override_func_def(deinit, LightPenDriver, Object)
);
