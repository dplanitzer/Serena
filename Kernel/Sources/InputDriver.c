//
//  InputDriver.c
//  Apollo
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "InputDriver.h"
#include "Heap.h"
#include "InterruptController.h"
#include "RingBuffer.h"


////////////////////////////////////////////////////////////////////////////////
// Keyboard Driver
////////////////////////////////////////////////////////////////////////////////


// 3,300bits/second at 25FPS
#define KEY_QUEUE_MAX_LENGTH    17


typedef struct _KeyboardDriver {
    EventDriverRef _Nonnull     event_driver;
    InterruptHandlerID          irq_handler;
    RingBuffer                  key_queue;
} KeyboardDriver;


extern void ksb_init(void);
extern Int ksb_receive_key(void);
extern void ksb_acknowledge_key(void);
static void KeyboardDriver_OnInterrupt(KeyboardDriverRef _Nonnull pDriver);


KeyboardDriverRef _Nullable KeyboardDriver_Create(EventDriverRef _Nonnull pEventDriver)
{
    KeyboardDriver* pDriver = (KeyboardDriver*)kalloc(sizeof(KeyboardDriver), HEAP_ALLOC_OPTION_CLEAR);
    FailNULL(pDriver);
    
    pDriver->event_driver = pEventDriver;
    pDriver->irq_handler = InterruptController_AddDirectInterruptHandler(InterruptController_GetShared(),
                                                                         INTERRUPT_ID_CIA_A_SP,
                                                                         INTERRUPT_HANDLER_PRIORITY_NORMAL,
                                                                         (InterruptHandler_Closure)KeyboardDriver_OnInterrupt,
                                                                         (Byte*)pDriver);
    FailZero(pDriver->irq_handler);
    
    FailFalse(RingBuffer_Init(&pDriver->key_queue, KEY_QUEUE_MAX_LENGTH));
    InterruptController_SetInterruptHandlerEnabled(InterruptController_GetShared(), pDriver->irq_handler, true);

    ksb_init();

    return pDriver;
    
failed:
    KeyboardDriver_Destroy(pDriver);
    return NULL;
}

void KeyboardDriver_Destroy(KeyboardDriverRef _Nullable pDriver)
{
    if (pDriver) {
        InterruptController_RemoveInterruptHandler(InterruptController_GetShared(), pDriver->irq_handler);
        RingBuffer_Deinit(&pDriver->key_queue);
        pDriver->event_driver = NULL;
        kfree((Byte*)pDriver);
    }
}

static void KeyboardDriver_OnInterrupt(KeyboardDriverRef _Nonnull pDriver)
{
    const UInt8 keycode = ksb_receive_key();
    
    if (RingBuffer_PutByte(&pDriver->key_queue, (Byte)keycode) == 0) {
        // The key queue is full. Flush it, push an overflow error and then the keycode
        RingBuffer_RemoveAll(&pDriver->key_queue);
        RingBuffer_PutByte(&pDriver->key_queue, 0xfa);
        RingBuffer_PutByte(&pDriver->key_queue, keycode);
    }
    ksb_acknowledge_key();
}

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

Bool KeyboardDriver_GetReport(KeyboardDriverRef _Nonnull pDriver, KeyboardReport* _Nonnull pReport)
{
    Byte keycode;
    
    const Int ips = cpu_disable_irqs();
    const Bool hasKey = RingBuffer_GetByte(&pDriver->key_queue, &keycode) > 0;
    cpu_restore_irqs(ips);
    
    if (hasKey) {
        pReport->keycode = (HIDKeyCode) gUSBHIDKeycodes[keycode & 0x7f];
        pReport->isKeyUp = keycode & 0x80;
        return true;
    } else {
        pReport->keycode = 0;
        return false;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Mouse Driver
////////////////////////////////////////////////////////////////////////////////
#if 0
#pragma mark -
#endif


typedef struct _MouseDriver {
    EventDriverRef _Nonnull event_driver;
    volatile UInt16*        reg_joydat;
    volatile UInt16*        reg_potgor;
    volatile UInt8*         reg_pra;
    Int16                   old_hcount;
    Int16                   old_vcount;
    UInt16                  right_button_mask;
    UInt16                  middle_button_mask;
    UInt8                   left_button_mask;
    Int8                    port;
} MouseDriver;


MouseDriverRef _Nullable MouseDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port)
{
    assert(port >= 0 && port <= 1);
    
    MouseDriver* pDriver = (MouseDriver*)kalloc(sizeof(MouseDriver), HEAP_ALLOC_OPTION_CLEAR);
    FailNULL(pDriver);
    
    pDriver->event_driver = pEventDriver;
    pDriver->reg_joydat = (port == 0) ? (volatile UInt16*)0xdff00a : (volatile UInt16*)0xdff00c;
    pDriver->reg_potgor = (volatile UInt16*)0xdff016;
    pDriver->reg_pra = (volatile UInt8*)0xbfe001;
    pDriver->right_button_mask = (port == 0) ? 0x400 : 0x4000;
    pDriver->middle_button_mask = (port == 0) ? 0x100 : 0x1000;
    pDriver->left_button_mask = (port == 0) ? 0x40 : 0x80;
    pDriver->port = (Int8)port;

    // Switch CIA PRA bit 7 and 6 to input for the left mouse button
    volatile UInt8* pDdra = (volatile UInt8*)0xbfe201;
    *pDdra &= 0x3f;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    volatile UInt8* pPotGo = (volatile UInt8*)0xdff034;
    *pPotGo &= 0x0f00;

    return pDriver;
    
failed:
    MouseDriver_Destroy(pDriver);
    return NULL;
}

void MouseDriver_Destroy(MouseDriverRef _Nullable pDriver)
{
    if (pDriver) {
        pDriver->event_driver = NULL;
        kfree((Byte*)pDriver);
    }
}

Bool MouseDriver_GetReport(MouseDriverRef _Nonnull pDriver, MouseReport* _Nonnull pReport)
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
    
    pReport->xDelta = xDelta;
    pReport->yDelta = yDelta;
    pReport->buttonsDown = 0;

    
    // Left mouse button
    register UInt8 pra = *(pDriver->reg_pra);
    if ((pra & pDriver->left_button_mask) == 0) {
        pReport->buttonsDown |= 0x01;
    }
    
    
    // Right mouse button
    register UInt16 potgor = *(pDriver->reg_potgor);
    if ((potgor & pDriver->right_button_mask) == 0) {
        pReport->buttonsDown |= 0x02;
    }

    
    // Middle mouse button
    if ((potgor & pDriver->middle_button_mask) == 0) {
        pReport->buttonsDown |= 0x04;
    }
    
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// Digital Joystick Driver
////////////////////////////////////////////////////////////////////////////////
#if 0
#pragma mark -
#endif


typedef struct _DigitalJoystickDriver {
    EventDriverRef _Nonnull event_driver;
    volatile UInt16*        reg_joydat;
    volatile UInt16*        reg_potgor;
    volatile UInt8*         reg_pra;
    UInt16                  right_button_mask;
    UInt8                   fire_button_mask;
    Int8                    port;
} DigitalJoystickDriver;


DigitalJoystickDriverRef _Nullable DigitalJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port)
{
    assert(port >= 0 && port <= 1);
    
    DigitalJoystickDriver* pDriver = (DigitalJoystickDriver*)kalloc(sizeof(DigitalJoystickDriver), HEAP_ALLOC_OPTION_CLEAR);
    FailNULL(pDriver);
    
    pDriver->event_driver = pEventDriver;
    pDriver->reg_joydat = (port == 0) ? (volatile UInt16*)0xdff00a : (volatile UInt16*)0xdff00c;
    pDriver->reg_potgor = (volatile UInt16*)0xdff016;
    pDriver->reg_pra = (volatile UInt8*)0xbfe001;
    pDriver->right_button_mask = (port == 0) ? 0x400 : 0x4000;
    pDriver->fire_button_mask = (port == 0) ? 0x40 : 0x80;
    pDriver->port = (Int8)port;
    
    // Switch bit 7 and 6 to input
    volatile UInt8* pDdra = (volatile UInt8*)0xbfe201;
    *pDdra &= 0x3f;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    volatile UInt8* pPotGo = (volatile UInt8*)0xdff034;
    *pPotGo &= 0x0f00;

    return pDriver;
    
failed:
    DigitalJoystickDriver_Destroy(pDriver);
    return NULL;
}

void DigitalJoystickDriver_Destroy(DigitalJoystickDriverRef _Nullable pDriver)
{
    if (pDriver) {
        pDriver->event_driver = NULL;
        kfree((Byte*)pDriver);
    }
}

Bool DigitalJoystickDriver_GetReport(DigitalJoystickDriverRef _Nonnull pDriver, JoystickReport* _Nonnull pReport)
{
    register UInt8 pra = *(pDriver->reg_pra);
    register UInt16 joydat = *(pDriver->reg_joydat);
    
    pReport->xAbs = 0;
    pReport->yAbs = 0;
    pReport->buttonsDown = 0;

    
    // Left fire button
    if ((pra & pDriver->fire_button_mask) == 0) {
        pReport->buttonsDown |= 0x01;
    }
    
    
    // Right fire button
    register UInt16 potgor = *(pDriver->reg_potgor);
    if ((potgor & pDriver->right_button_mask) == 0) {
        pReport->buttonsDown |= 0x02;
    }

    
    // X axis
    if ((joydat & (1 << 1)) != 0) {
        pReport->xAbs = INT16_MAX;  // right
    } else if ((joydat & (1 << 9)) != 0) {
        pReport->xAbs = INT16_MIN;  // left
    }
    
    
    // Y axis
    register UInt16 joydat_xor = joydat ^ (joydat >> 1);
    if ((joydat & (1 << 0)) != 0) {
        pReport->yAbs = INT16_MAX;  // down
    } else if ((joydat & (1 << 8)) != 0) {
        pReport->yAbs = INT16_MIN;  // up
    }

    return true;
}


////////////////////////////////////////////////////////////////////////////////
// Analog Joystick (Paddles) Driver
////////////////////////////////////////////////////////////////////////////////
#if 0
#pragma mark -
#endif


typedef struct _AnalogJoystickDriver {
    EventDriverRef _Nonnull event_driver;
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
} AnalogJoystickDriver;


AnalogJoystickDriverRef _Nullable AnalogJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port)
{
    assert(port >= 0 && port <= 1);
    
    AnalogJoystickDriver* pDriver = (AnalogJoystickDriver*)kalloc(sizeof(AnalogJoystickDriver), HEAP_ALLOC_OPTION_CLEAR);
    FailNULL(pDriver);
    
    pDriver->event_driver = pEventDriver;
    pDriver->reg_joydat = (port == 0) ? (volatile UInt16*)0xdff00a : (volatile UInt16*)0xdff00c;
    pDriver->reg_potdat = (port == 0) ? (volatile UInt16*)0xdff012 : (volatile UInt16*)0xdff014;
    pDriver->reg_potgo = (volatile UInt16*)0xdff034;
    pDriver->port = (Int8)port;
    pDriver->sampleCount = 4;
    pDriver->sampleIndex = 0;
    pDriver->sumX = 0;
    pDriver->sumY = 0;
    pDriver->smoothedX = 0;
    pDriver->smoothedY = 0;
    
    return pDriver;
    
failed:
    AnalogJoystickDriver_Destroy(pDriver);
    return NULL;
}

void AnalogJoystickDriver_Destroy(AnalogJoystickDriverRef _Nullable pDriver)
{
    if (pDriver) {
        pDriver->event_driver = NULL;
        kfree((Byte*)pDriver);
    }
}

Bool AnalogJoystickDriver_GetReport(AnalogJoystickDriverRef _Nonnull pDriver, JoystickReport* _Nonnull pReport)
{
    register UInt16 potdat = *(pDriver->reg_potdat);
    register UInt16 joydat = *(pDriver->reg_joydat);

    // Return the smoothed value
    pReport->xAbs = pDriver->smoothedX;
    pReport->yAbs = pDriver->smoothedY;
    pReport->buttonsDown = 0;
    
    
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
        pReport->buttonsDown |= 0x01;
    }
    
    
    // Right fire button
    if ((joydat & (1 << 1)) != 0) {
        pReport->buttonsDown |= 0x02;
    }
    
    
    // Restart the counter for the next frame
    *(pDriver->reg_potgo) = 0x0001;
    
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// Light Pen Driver
////////////////////////////////////////////////////////////////////////////////
#if 0
#pragma mark -
#endif


typedef struct _LightPenDriver {
    EventDriverRef _Nonnull event_driver;
    volatile UInt16*        reg_potgor;
    UInt16                  right_button_mask;
    UInt16                  middle_button_mask;
    Int16                   smoothedX;
    Int16                   smoothedY;
    Bool                    hasSmoothedPosition;
    Int16                   sumX;
    Int16                   sumY;
    Int8                    sampleCount;    // How many samples to average to produce a smoothed value
    Int8                    sampleIndex;    // Current sample in the range 0..<sampleCount
    Int8                    triggerCount;   // Number of times that the light pen has triggered in the 'sampleCount' interval
    Int8                    port;
} LightPenDriver;


LightPenDriverRef _Nullable LightPenDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port)
{
    assert(port >= 0 && port <= 1);
    
    LightPenDriver* pDriver = (LightPenDriver*)kalloc(sizeof(LightPenDriver), HEAP_ALLOC_OPTION_CLEAR);
    FailNULL(pDriver);
    
    pDriver->event_driver = pEventDriver;
    pDriver->reg_potgor = (volatile UInt16*)0xdff016;
    pDriver->right_button_mask = (port == 0) ? 0x400 : 0x4000;
    pDriver->middle_button_mask = (port == 0) ? 0x100 : 0x1000;
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
    volatile UInt8* pPotGo = (volatile UInt8*)0xdff034;
    *pPotGo &= 0x0f00;
    
    return pDriver;
    
failed:
    LightPenDriver_Destroy(pDriver);
    return NULL;
}

void LightPenDriver_Destroy(LightPenDriverRef _Nullable pDriver)
{
    if (pDriver) {
        pDriver->event_driver = NULL;
        kfree((Byte*)pDriver);
    }
}

Bool LightPenDriver_GetReport(LightPenDriverRef _Nonnull pDriver, LightPenReport* _Nonnull pReport)
{
    GraphicsDriverRef pGraphicsDriver = EventDriver_GetGraphicsDriver(pDriver->event_driver);
    
    // Return the smoothed value
    pReport->xAbs = pDriver->smoothedX;
    pReport->yAbs = pDriver->smoothedY;
    pReport->hasPosition = pDriver->hasSmoothedPosition;
    pReport->buttonsDown = 0;

    
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
        
        if (GraphicsDriver_GetLightPenPosition(pGraphicsDriver, &xPos, &yPos)) {
            pDriver->triggerCount++;
            pDriver->sumX += xPos;
            pDriver->sumY += yPos;
        }
    }

    
    // Button #0
    register UInt16 potgor = *(pDriver->reg_potgor);
    if ((potgor & pDriver->right_button_mask) == 0) {
        pReport->buttonsDown |= 0x02;
    }
    
    
    // Button # 1
    if ((potgor & pDriver->middle_button_mask) == 0) {
        pReport->buttonsDown |= 0x04;
    }
    
    return true;
}
