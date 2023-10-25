//
//  InputDriver.c
//  Apollo
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "InputDriver.h"
#include "HIDKeyRepeater.h"
#include "InterruptController.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Keyboard Driver
////////////////////////////////////////////////////////////////////////////////

// Keycode -> USB HID keyscan codes
// See: <http://whdload.de/docs/en/rawkey.html>
// See: <http://www.quadibloc.com/comp/scan.htm>
static const UInt8 gUSBHIDKeycodes[128] = {
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x31, 0x00, 0x62, // $00 - $0f
    0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x00, 0x59, 0x5a, 0x5b, // $10 - $1f
    0x04, 0x16, 0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x00, 0x00, 0x5c, 0x5d, 0x5e, // $20 - $2f
    0x36, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0x00, 0x63, 0x5f, 0x60, 0x61, // $30 - $3f
    0x2c, 0x2a, 0x2b, 0x58, 0x28, 0x29, 0x4c, 0x00, 0x00, 0x00, 0x56, 0x00, 0x52, 0x51, 0x4f, 0x50, // $40 - $4f
    0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x53, 0x54, 0x55, 0x56, 0x57, 0x75, // $50 - $5f
    0xe1, 0xe5, 0x39, 0xe0, 0xe2, 0xe6, 0xe3, 0xe7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $60 - $6f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, // $70 - $7f
};


CLASS_INTERFACE(KeyboardDriver, IOResource,
    const UInt8* _Nonnull       keyCodeMap;
    EventDriverRef _Nonnull     eventDriver;
    HIDKeyRepeaterRef _Nonnull  keyRepeater;
    InterruptHandlerID          keyboardIrqHandler;
    InterruptHandlerID          vblIrqHandler;
);


extern void ksb_init(void);
extern Int ksb_receive_key(void);
extern void ksb_acknowledge_key(void);
extern void KeyboardDriver_OnKeyboardInterrupt(KeyboardDriverRef _Nonnull pDriver);
extern void KeyboardDriver_OnVblInterrupt(KeyboardDriverRef _Nonnull pDriver);


ErrorCode KeyboardDriver_Create(EventDriverRef _Nonnull pEventDriver, KeyboardDriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    KeyboardDriver* pDriver;
    
    try(Object_Create(KeyboardDriver, &pDriver));
    
    pDriver->keyCodeMap = gUSBHIDKeycodes;
    pDriver->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    try(HIDKeyRepeater_Create(pEventDriver, &pDriver->keyRepeater));

    ksb_init();

    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_CIA_A_SP,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL,
                                                      (InterruptHandler_Closure)KeyboardDriver_OnKeyboardInterrupt,
                                                      (Byte*)pDriver,
                                                      &pDriver->keyboardIrqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, pDriver->keyboardIrqHandler, true);

    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 1,
                                                      (InterruptHandler_Closure)KeyboardDriver_OnVblInterrupt,
                                                      (Byte*)pDriver,
                                                      &pDriver->vblIrqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, pDriver->vblIrqHandler, true);

    *pOutDriver = pDriver;
    return EOK;

catch:
    Object_Release(pDriver);
    *pOutDriver = NULL;
    return err;
}

static void KeyboardDriver_deinit(KeyboardDriverRef _Nonnull pDriver)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, pDriver->keyboardIrqHandler));
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, pDriver->vblIrqHandler));

    HIDKeyRepeater_Destroy(pDriver->keyRepeater);
    pDriver->keyRepeater = NULL;

    Object_Release(pDriver->eventDriver);
    pDriver->eventDriver = NULL;
}

void KeyboardDriver_GetKeyRepeatDelays(KeyboardDriverRef _Nonnull pDriver, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay)
{
    const Int irs = cpu_disable_irqs();
    HIDKeyRepeater_GetKeyRepeatDelays(pDriver->keyRepeater, pInitialDelay, pRepeatDelay);
    cpu_restore_irqs(irs);
}

void KeyboardDriver_SetKeyRepeatDelays(KeyboardDriverRef _Nonnull pDriver, TimeInterval initialDelay, TimeInterval repeatDelay)
{
    const Int irs = cpu_disable_irqs();
    HIDKeyRepeater_SetKeyRepeatDelays(pDriver->keyRepeater, initialDelay, repeatDelay);
    cpu_restore_irqs(irs);
}

void KeyboardDriver_OnKeyboardInterrupt(KeyboardDriverRef _Nonnull pDriver)
{
    const UInt8 keyCode = ksb_receive_key();
    const HIDKeyState state = (keyCode & 0x80) ? kHIDKeyState_Up : kHIDKeyState_Down;
    const UInt16 code = (UInt16)pDriver->keyCodeMap[keyCode & 0x7f];

    if (code > 0) {
        EventDriver_ReportKeyboardDeviceChange(pDriver->eventDriver, state, code);
        if (state == kHIDKeyState_Up) {
            HIDKeyRepeater_KeyUp(pDriver->keyRepeater, code);
        } else {
            HIDKeyRepeater_KeyDown(pDriver->keyRepeater, code);
        }
    }
    ksb_acknowledge_key();
}

void KeyboardDriver_OnVblInterrupt(KeyboardDriverRef _Nonnull pDriver)
{
    // const Int = cpu_disable_irqs();
    HIDKeyRepeater_Tick(pDriver->keyRepeater);
    // cpu_restore_irqs(irs);
}


CLASS_IMPLEMENTATION(KeyboardDriver, IOResource,
OVERRIDE_METHOD_IMPL(deinit, KeyboardDriver, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Driver
////////////////////////////////////////////////////////////////////////////////

CLASS_INTERFACE(MouseDriver, IOResource,
    EventDriverRef _Nonnull eventDriver;
    InterruptHandlerID      irqHandler;
    volatile UInt16*        reg_joydat;
    volatile UInt16*        reg_potgor;
    volatile UInt8*         reg_ciaa_pra;
    Int16                   old_hcount;
    Int16                   old_vcount;
    UInt16                  right_button_mask;
    UInt16                  middle_button_mask;
    UInt8                   left_button_mask;
    Int8                    port;
);

extern void MouseDriver_OnInterrupt(MouseDriverRef _Nonnull pDriver);


ErrorCode MouseDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, MouseDriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    CIAA_BASE_DECL(ciaa);
    MouseDriver* pDriver = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Object_Create(MouseDriver, &pDriver));
    
    pDriver->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    pDriver->reg_joydat = (port == 0) ? CHIPSET_REG_16(cp, JOY0DAT) : CHIPSET_REG_16(cp, JOY1DAT);
    pDriver->reg_potgor = CHIPSET_REG_16(cp, POTGOR);
    pDriver->reg_ciaa_pra = CIA_REG_8(ciaa, 0);
    pDriver->right_button_mask = (port == 0) ? POTGORF_DATLY : POTGORF_DATRY;
    pDriver->middle_button_mask = (port == 0) ? POTGORF_DATLX : POTGORF_DATRX;
    pDriver->left_button_mask = (port == 0) ? CIAA_PRAF_FIR0 : CIAA_PRAF_FIR1;
    pDriver->port = (Int8)port;

    // Switch CIA PRA bit 7 and 6 to input for the left mouse button
    *CIA_REG_8(ciaa, CIA_DDRA) = *CIA_REG_8(ciaa, CIA_DDRA) & 0x3f;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;

    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 2,
                                                      (InterruptHandler_Closure)MouseDriver_OnInterrupt,
                                                      (Byte*)pDriver,
                                                      &pDriver->irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, pDriver->irqHandler, true);

    *pOutDriver = pDriver;
    return EOK;
    
catch:
    Object_Release(pDriver);
    *pOutDriver = NULL;
    return err;
}

static void MouseDriver_deinit(MouseDriverRef _Nonnull pDriver)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, pDriver->irqHandler));

    Object_Release(pDriver->eventDriver);
    pDriver->eventDriver = NULL;
}

void MouseDriver_OnInterrupt(MouseDriverRef _Nonnull pDriver)
{
    register UInt16 new_state = *(pDriver->reg_joydat);
    
    // X delta
    register Int16 new_x = (Int16)(new_state & 0x00ff);
    register Int16 xDelta = new_x - pDriver->old_hcount;
    pDriver->old_hcount = new_x;
    
    if (xDelta < -127) {
        // underflow
        xDelta = -255 - xDelta;
        if (xDelta < 0) xDelta = 0;
    } else if (xDelta > 127) {
        xDelta = 255 - xDelta;
        if (xDelta >= 0) xDelta = 0;
    }
    
    
    // Y delta
    register Int16 new_y = (Int16)((new_state & 0xff00) >> 8);
    register Int16 yDelta = new_y - pDriver->old_vcount;
    pDriver->old_vcount = new_y;
    
    if (yDelta < -127) {
        // underflow
        yDelta = -255 - yDelta;
        if (yDelta < 0) yDelta = 0;
    } else if (yDelta > 127) {
        yDelta = 255 - yDelta;
        if (yDelta >= 0) yDelta = 0;
    }

    
    // Left mouse button
    register UInt32 buttonsDown = 0;
    register UInt8 pra = *(pDriver->reg_ciaa_pra);
    if ((pra & pDriver->left_button_mask) == 0) {
        buttonsDown |= 0x01;
    }
    
    
    // Right mouse button
    register UInt16 potgor = *(pDriver->reg_potgor);
    if ((potgor & pDriver->right_button_mask) == 0) {
        buttonsDown |= 0x02;
    }

    
    // Middle mouse button
    if ((potgor & pDriver->middle_button_mask) == 0) {
        buttonsDown |= 0x04;
    }


    EventDriver_ReportMouseDeviceChange(pDriver->eventDriver, xDelta, yDelta, buttonsDown);
}


CLASS_IMPLEMENTATION(MouseDriver, IOResource,
OVERRIDE_METHOD_IMPL(deinit, MouseDriver, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Digital Joystick Driver
////////////////////////////////////////////////////////////////////////////////

CLASS_INTERFACE(DigitalJoystickDriver, IOResource,
    EventDriverRef _Nonnull eventDriver;
    InterruptHandlerID      irqHandler;
    volatile UInt16*        reg_joydat;
    volatile UInt16*        reg_potgor;
    volatile UInt8*         reg_ciaa_pra;
    UInt16                  right_button_mask;
    UInt8                   fire_button_mask;
    Int8                    port;
);

extern void DigitalJoystickDriver_OnInterrupt(DigitalJoystickDriverRef _Nonnull pDriver);


ErrorCode DigitalJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, DigitalJoystickDriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    CIAA_BASE_DECL(ciaa);
    DigitalJoystickDriver* pDriver = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Object_Create(DigitalJoystickDriver, &pDriver));
    
    pDriver->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    pDriver->reg_joydat = (port == 0) ? CHIPSET_REG_16(cp, JOY0DAT) : CHIPSET_REG_16(cp, JOY1DAT);
    pDriver->reg_potgor = CHIPSET_REG_16(cp, POTGOR);
    pDriver->reg_ciaa_pra = CIA_REG_8(ciaa, 0);
    pDriver->right_button_mask = (port == 0) ? POTGORF_DATLY : POTGORF_DATRY;
    pDriver->fire_button_mask = (port == 0) ? CIAA_PRAF_FIR0 : CIAA_PRAF_FIR1;
    pDriver->port = (Int8)port;
    
    // Switch bit 7 and 6 to input
    *CIA_REG_8(ciaa, CIA_DDRA) = *CIA_REG_8(ciaa, CIA_DDRA) & 0x3f;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;

    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 1,
                                                      (InterruptHandler_Closure)DigitalJoystickDriver_OnInterrupt,
                                                      (Byte*)pDriver,
                                                      &pDriver->irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, pDriver->irqHandler, true);

    *pOutDriver = pDriver;
    return EOK;
    
catch:
    Object_Release(pDriver);
    *pOutDriver = NULL;
    return err;
}

static void DigitalJoystickDriver_deinit(DigitalJoystickDriverRef _Nonnull pDriver)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, pDriver->irqHandler));
    
    Object_Release(pDriver->eventDriver);
    pDriver->eventDriver = NULL;
}

void DigitalJoystickDriver_OnInterrupt(DigitalJoystickDriverRef _Nonnull pDriver)
{
    register UInt8 pra = *(pDriver->reg_ciaa_pra);
    register UInt16 joydat = *(pDriver->reg_joydat);
    Int16 xAbs, yAbs;
    UInt32 buttonsDown = 0;

    
    // Left fire button
    if ((pra & pDriver->fire_button_mask) == 0) {
        buttonsDown |= 0x01;
    }
    
    
    // Right fire button
    register UInt16 potgor = *(pDriver->reg_potgor);
    if ((potgor & pDriver->right_button_mask) == 0) {
        buttonsDown |= 0x02;
    }

    
    // X axis
    if ((joydat & (1 << 1)) != 0) {
        xAbs = INT16_MAX;  // right
    } else if ((joydat & (1 << 9)) != 0) {
        xAbs = INT16_MIN;  // left
    }
    
    
    // Y axis
    register UInt16 joydat_xor = joydat ^ (joydat >> 1);
    if ((joydat & (1 << 0)) != 0) {
        yAbs = INT16_MAX;  // down
    } else if ((joydat & (1 << 8)) != 0) {
        yAbs = INT16_MIN;  // up
    }


    EventDriver_ReportJoystickDeviceChange(pDriver->eventDriver, pDriver->port, xAbs, yAbs, buttonsDown);
}


CLASS_IMPLEMENTATION(DigitalJoystickDriver, IOResource,
OVERRIDE_METHOD_IMPL(deinit, DigitalJoystickDriver, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Analog Joystick (Paddles) Driver
////////////////////////////////////////////////////////////////////////////////

CLASS_INTERFACE(AnalogJoystickDriver, IOResource,
    EventDriverRef _Nonnull eventDriver;
    InterruptHandlerID      irqHandler;
    volatile UInt16*        reg_joydat;
    volatile UInt16*        reg_potdat;
    volatile UInt16*        reg_potgo;
    Int16                   smoothedX;
    Int16                   smoothedY;
    Int16                   sumX;
    Int16                   sumY;
    Int8                    sampleCount;    // How many samples to average to produce a smoothed value
    Int8                    sampleIndex;    // Current sample in the range 0..<sampleCount
    Int8                    port;
);

extern void AnalogJoystickDriver_OnInterrupt(AnalogJoystickDriverRef _Nonnull pDriver);


ErrorCode AnalogJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, AnalogJoystickDriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    AnalogJoystickDriver* pDriver = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Object_Create(AnalogJoystickDriver, &pDriver));
    
    pDriver->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    pDriver->reg_joydat = (port == 0) ? CHIPSET_REG_16(cp, JOY0DAT) : CHIPSET_REG_16(cp, JOY1DAT);
    pDriver->reg_potdat = (port == 0) ? CHIPSET_REG_16(cp, POT0DAT) : CHIPSET_REG_16(cp, POT1DAT);
    pDriver->reg_potgo = CHIPSET_REG_16(cp, POTGO);
    pDriver->port = (Int8)port;
    pDriver->sampleCount = 4;
    pDriver->sampleIndex = 0;
    pDriver->sumX = 0;
    pDriver->sumY = 0;
    pDriver->smoothedX = 0;
    pDriver->smoothedY = 0;
    
    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 1,
                                                      (InterruptHandler_Closure)AnalogJoystickDriver_OnInterrupt,
                                                      (Byte*)pDriver,
                                                      &pDriver->irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, pDriver->irqHandler, true);

    *pOutDriver = pDriver;
    return EOK;
    
catch:
    Object_Release(pDriver);
    *pOutDriver = NULL;
    return err;
}

static void AnalogJoystickDriver_deinit(AnalogJoystickDriverRef _Nonnull pDriver)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, pDriver->irqHandler));
    
    Object_Release(pDriver->eventDriver);
    pDriver->eventDriver = NULL;
}

void AnalogJoystickDriver_OnInterrupt(AnalogJoystickDriverRef _Nonnull pDriver)
{
    register UInt16 potdat = *(pDriver->reg_potdat);
    register UInt16 joydat = *(pDriver->reg_joydat);

    // Return the smoothed value
    const Int16 xAbs = pDriver->smoothedX;
    const Int16 yAbs = pDriver->smoothedY;
    UInt32 buttonsDown = 0;
    
    
    // Sum up to 'sampleCount' samples and then compute the smoothed out value
    // as the average of 'sampleCount' samples.
    if (pDriver->sampleIndex == pDriver->sampleCount) {
        pDriver->smoothedX = (pDriver->sumX / pDriver->sampleCount) << 8;
        pDriver->smoothedY = (pDriver->sumY / pDriver->sampleCount) << 8;
        pDriver->sampleIndex = 0;
        pDriver->sumX = 0;
        pDriver->sumY = 0;
    } else {
        pDriver->sampleIndex++;
        
        // X axis
        const Int16 xval = (Int16)(potdat & 0x00ff);
        pDriver->sumX += (xval - 128);
        
        
        // Y axis
        const Int16 yval = (Int16)((potdat >> 8) & 0x00ff);
        pDriver->sumY += (yval - 128);
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
    *(pDriver->reg_potgo) = 0x0001;
    
    
    EventDriver_ReportJoystickDeviceChange(pDriver->eventDriver, pDriver->port, xAbs, yAbs, buttonsDown);
}


CLASS_IMPLEMENTATION(AnalogJoystickDriver, IOResource,
OVERRIDE_METHOD_IMPL(deinit, AnalogJoystickDriver, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Light Pen Driver
////////////////////////////////////////////////////////////////////////////////

CLASS_INTERFACE(LightPenDriver, IOResource,
    EventDriverRef _Nonnull     eventDriver;
    GraphicsDriverRef _Nonnull  gdevice;
    InterruptHandlerID          irqHandler;
    volatile UInt16*            reg_potgor;
    UInt16                      right_button_mask;
    UInt16                      middle_button_mask;
    Int16                       smoothedX;
    Int16                       smoothedY;
    Bool                        hasSmoothedPosition;    // True if the light pen position is available (pen triggered the position latching hardware); false otherwise
    Int16                       sumX;
    Int16                       sumY;
    Int8                        sampleCount;    // How many samples to average to produce a smoothed value
    Int8                        sampleIndex;    // Current sample in the range 0..<sampleCount
    Int8                        triggerCount;   // Number of times that the light pen has triggered in the 'sampleCount' interval
    Int8                        port;
);

extern void LightPenDriver_OnInterrupt(LightPenDriverRef _Nonnull pDriver);


ErrorCode LightPenDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, LightPenDriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    LightPenDriver* pDriver = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Object_Create(LightPenDriver, &pDriver));
    
    pDriver->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    pDriver->gdevice = Object_RetainAs(EventDriver_GetGraphicsDriver(pEventDriver), GraphicsDriver);
    pDriver->reg_potgor = CHIPSET_REG_16(cp, POTGOR);
    pDriver->right_button_mask = (port == 0) ? POTGORF_DATLY : POTGORF_DATRY;
    pDriver->middle_button_mask = (port == 0) ? POTGORF_DATLX : POTGORF_DATRX;
    pDriver->smoothedX = 0;
    pDriver->smoothedY = 0;
    pDriver->sumX = 0;
    pDriver->sumY = 0;
    pDriver->hasSmoothedPosition = false;
    pDriver->sampleCount = 4;
    pDriver->sampleIndex = 0;
    pDriver->triggerCount = 0;
    pDriver->port = (Int8)port;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;
    
    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 1,
                                                      (InterruptHandler_Closure)LightPenDriver_OnInterrupt,
                                                      (Byte*)pDriver,
                                                      &pDriver->irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, pDriver->irqHandler, true);

    *pOutDriver = pDriver;
    return EOK;
    
catch:
    Object_Release(pDriver);
    *pOutDriver = NULL;
    return err;
}

static void LightPenDriver_deinit(LightPenDriverRef _Nonnull pDriver)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, pDriver->irqHandler));
    
    Object_Release(pDriver->gdevice);
    pDriver->gdevice = NULL;

    Object_Release(pDriver->eventDriver);
    pDriver->eventDriver = NULL;
}

void LightPenDriver_OnInterrupt(LightPenDriverRef _Nonnull pDriver)
{
    // Return the smoothed value
    Int16 xAbs = pDriver->smoothedX;
    Int16 yAbs = pDriver->smoothedY;
    const Bool hasPosition = pDriver->hasSmoothedPosition;
    UInt32 buttonsDown = 0;

    
    // Sum up to 'sampleCount' samples and then compute the smoothed out value
    // as the average of 'sampleCount' samples.
    if (pDriver->sampleIndex == pDriver->sampleCount) {
        pDriver->smoothedX = pDriver->triggerCount ? (pDriver->sumX / pDriver->triggerCount) << 8 : 0;
        pDriver->smoothedY = pDriver->triggerCount ? (pDriver->sumY / pDriver->triggerCount) << 8 : 0;
        pDriver->hasSmoothedPosition = pDriver->triggerCount >= (pDriver->sampleCount / 2);
        pDriver->sampleIndex = 0;
        pDriver->triggerCount = 0;
        pDriver->sumX = 0;
        pDriver->sumY = 0;
    } else {
        pDriver->sampleIndex++;
    
        // Get the position
        Int16 xPos, yPos;
        
        if (GraphicsDriver_GetLightPenPosition(pDriver->gdevice, &xPos, &yPos)) {
            pDriver->triggerCount++;
            pDriver->sumX += xPos;
            pDriver->sumY += yPos;
        }
    }

    
    // Button #0
    register UInt16 potgor = *(pDriver->reg_potgor);
    if ((potgor & pDriver->right_button_mask) == 0) {
        buttonsDown |= 0x02;
    }
    
    
    // Button # 1
    if ((potgor & pDriver->middle_button_mask) == 0) {
        buttonsDown |= 0x04;
    }
    

    EventDriver_ReportLightPenDeviceChange(pDriver->eventDriver, xAbs, yAbs, hasPosition, buttonsDown);
}


CLASS_IMPLEMENTATION(LightPenDriver, IOResource,
OVERRIDE_METHOD_IMPL(deinit, LightPenDriver, Object)
);
