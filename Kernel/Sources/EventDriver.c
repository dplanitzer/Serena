//
//  EventDriver.c
//  Apollo
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "EventDriverPriv.h"

//extern const UInt16 gArrow_Bits[];
//extern const UInt16 gArrow_Mask[];

// USB Keycode -> kHIDEventModifierFlag_XXX values which are be or'd / and'd into pDriver->modifierFlags.
// Bit 7 indicates whether the key is left or right: 0 -> left; 1 -> right
// shift    0x01
// option   0x02
// ctrl     0x04
// command  0x08
// capslock 0x10
// keypad   0x20
// func     0x40
// isRight  0x80
static const UInt8 gUSBHIDKeyFlags[256] = {
    0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $00 - $0f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $10 - $1f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, // $20 - $2f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $30 - $3f
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $40 - $4f
    0x40, 0x40, 0x40, 0x60, 0x20, 0x20, 0x20, 0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // $50 - $5f
    0x20, 0x20, 0x20, 0x20, 0x00, 0x40, 0x40, 0x20, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $60 - $6f
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $70 - $7f
    
    0x40, 0x40, 0x40, 0x40, 0x40, 0x20, 0x20, 0x40, 0x40, 0x40, 0x40, 0x40, 0x20, 0x40, 0x40, 0x40, // $80 - $8f
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $90 - $9f
    0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $a0 - $af
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x60, 0x60, 0x20, 0x20, 0x20, 0x20, // $b0 - $bf
    0x20, 0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // $c0 - $cf
    0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, // $d0 - $df
    0x04, 0x01, 0x02, 0x08, 0x84, 0x81, 0x82, 0x88, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $e0 - $ef
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, // $f0 - $ff
};


ErrorCode EventDriver_Create(GraphicsDriverRef _Nonnull gdevice, EventDriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    EventDriver* pDriver;
    
    try(kalloc_cleared(sizeof(EventDriver), (Byte**) &pDriver));

    Lock_Init(&pDriver->lock);
    pDriver->graphicsDriver = gdevice;

    pDriver->keyFlags = gUSBHIDKeyFlags;

    pDriver->screenLeft = 0;
    pDriver->screenTop = 0;
    pDriver->screenRight = (Int16) GraphicsDriver_GetFramebufferSize(gdevice).width;
    pDriver->screenBottom = (Int16) GraphicsDriver_GetFramebufferSize(gdevice).height;
    pDriver->mouseCursorHiddenCounter = 1;
    pDriver->isMouseMoveReportingEnabled = false;

    pDriver->modifierFlags = 0;

    pDriver->mouseX = 0;
    pDriver->mouseY = 0;
    pDriver->mouseButtons = 0;


    // Create the HID event queue
    try(HIDEventQueue_Create(REPORT_QUEUE_MAX_EVENTS, &pDriver->eventQueue));


    // Open the keyboard driver
    try(KeyboardDriver_Create(pDriver, &pDriver->keyboardDriver));


    // Open the mouse/joystick/light pen driver
    for (Int i = 0; i < MAX_INPUT_CONTROLLER_PORTS; i++) {
        pDriver->port[i].type = kInputControllerType_None;
    }
    try(EventDriver_CreateInputControllerForPort(pDriver, kInputControllerType_Mouse, 0));


    // XXX
    //GraphicsDriver_SetMouseCursor(gdevice, (Byte*)gArrow_Bits, (Byte*)gArrow_Mask);
    //EventDriver_ShowMouseCursor(pDriver);
    // XXX

    *pOutDriver = pDriver;
    return EOK;
    
catch:
    EventDriver_Destroy(pDriver);
    *pOutDriver = NULL;
    return err;
}

void EventDriver_Destroy(EventDriverRef _Nullable pDriver)
{
    if (pDriver) {
        for (Int i = 0; i < MAX_INPUT_CONTROLLER_PORTS; i++) {
            EventDriver_DestroyInputControllerForPort(pDriver, i);
        }
        KeyboardDriver_Destroy(pDriver->keyboardDriver);
        pDriver->keyboardDriver = NULL;
        HIDEventQueue_Destroy(pDriver->eventQueue);
        pDriver->graphicsDriver = NULL;
        Lock_Deinit(&pDriver->lock);

        kfree((Byte*)pDriver);
    }
}

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Input Driver API
////////////////////////////////////////////////////////////////////////////////

GraphicsDriverRef _Nonnull EventDriver_GetGraphicsDriver(EventDriverRef _Nonnull pDriver)
{
    return pDriver->graphicsDriver;
}

// Reports a key down, repeat or up from a keyboard device. This function updates
// the state of the logical keyboard and it posts a suitable keyboard event to
// the event queue.
// Must be called from the interrupt context with interrupts turned off.
void EventDriver_ReportKeyboardDeviceChange(EventDriverRef _Nonnull pDriver, HIDKeyState keyState, UInt16 keyCode)
{
    // Update the key map
    const UInt32 wordIdx = keyCode >> 5;
    const UInt32 bitIdx = keyCode - (wordIdx << 5);

    if (keyState == kHIDKeyState_Up) {
        pDriver->keyMap[wordIdx] &= ~(1 << bitIdx);
    } else {
        pDriver->keyMap[wordIdx] |= (1 << bitIdx);
    }


    // Update the modifier flags
    const UInt32 logModFlags = (keyCode <= 255) ? pDriver->keyFlags[keyCode] & 0x1f : 0;
    const Bool isModifierKey = (logModFlags != 0);    
    UInt32 modifierFlags = pDriver->modifierFlags;

    if (isModifierKey) {
        const Bool isRight = (keyCode <= 255) ? pDriver->keyFlags[keyCode] & 0x80 : 0;
        const UInt32 devModFlags = (isRight) ? logModFlags << 16 : logModFlags << 24;

        if (keyState == kHIDKeyState_Up) {
            modifierFlags &= ~logModFlags;
            modifierFlags &= ~devModFlags;
        } else {
            modifierFlags |= logModFlags;
            modifierFlags |= devModFlags;
        }
        pDriver->modifierFlags = modifierFlags;
    }


    // Generate and post the keyboard event
    const UInt32 keyFunc = (keyCode <= 255) ? pDriver->keyFlags[keyCode] & 0x60 : 0;
    const UInt32 flags = modifierFlags | keyFunc;
    HIDEventType evtType;
    HIDEventData evt;

    if (!isModifierKey) {
        evtType = (keyState == kHIDKeyState_Up) ? kHIDEventType_KeyUp : kHIDEventType_KeyDown;
    } else {
        evtType = kHIDEventType_FlagsChanged;
    }
    evt.key.flags = flags;
    evt.key.keyCode = keyCode;
    evt.key.isRepeat = false;

    HIDEventQueue_Put(pDriver->eventQueue, evtType, &evt);
}

// Reports a change in the state of a mouse device. Updates the state of the
// logical mouse device and posts suitable events to the event queue.
// Must be called from the interrupt context with interrupts turned off.
// \param xDelta change in mouse position X since last invocation
// \param yDelta change in mouse posution Y since last invocation
// \param buttonsDown absolute state of the mouse buttons (0 -> left button, 1 -> right button, 2-> middle button, ...) 
void EventDriver_ReportMouseDeviceChange(EventDriverRef _Nonnull pDriver, Int16 xDelta, Int16 yDelta, UInt32 buttonsDown)
{
    const UInt32 oldButtonsDown = pDriver->mouseButtons;
    const Bool hasButtonsChange = (oldButtonsDown != buttonsDown);
    const Bool hasPositionChange = (xDelta != 0 || yDelta != 0);

    if (hasPositionChange) {
        pDriver->mouseX += xDelta;
        pDriver->mouseY += yDelta;
        pDriver->mouseX = __min(__max(pDriver->mouseX, pDriver->screenLeft), pDriver->screenRight);
        pDriver->mouseY = __min(__max(pDriver->mouseY, pDriver->screenTop), pDriver->screenBottom);

        GraphicsDriver_SetMouseCursorPositionFromInterruptContext(pDriver->graphicsDriver, pDriver->mouseX, pDriver->mouseY);
    }
    pDriver->mouseButtons = buttonsDown;


    if (hasButtonsChange) {
        HIDEventType evtType;
        HIDEventData evt;

        // Generate mouse button up/down events
        // XXX should be able to ask the mouse input driver how many buttons it supports
        for (Int i = 0; i < 3; i++) {
            const UInt32 old_down = oldButtonsDown & (1 << i);
            const UInt32 new_down = buttonsDown & (1 << i);
            
            if (old_down ^ new_down) {
                if (old_down == 0 && new_down != 0) {
                    evtType = kHIDEventType_MouseDown;
                } else {
                    evtType = kHIDEventType_MouseUp;
                }
                evt.mouse.buttonNumber = i;
                evt.mouse.flags = pDriver->modifierFlags;
                evt.mouse.location.x = pDriver->mouseX;
                evt.mouse.location.y = pDriver->mouseY;
                HIDEventQueue_Put(pDriver->eventQueue, evtType, &evt);
            }
        }
    }
    else if (hasPositionChange && pDriver->isMouseMoveReportingEnabled) {
        HIDEventData evt;

        evt.mouseMoved.flags = pDriver->modifierFlags;
        evt.mouseMoved.location.x = pDriver->mouseX;
        evt.mouseMoved.location.y = pDriver->mouseY;
        HIDEventQueue_Put(pDriver->eventQueue, kHIDEventType_MouseMoved, &evt);
    }
}


// Reports a change in the state of a light pen device. Posts suitable events to
// the event queue. The light pen controls the mouse cursor and generates mouse
// events.
// Must be called from the interrupt context with interrupts turned off.
// \param xAbs absolute light pen X coordinate
// \param yAbs absolute light pen Y coordinate
// \param hasPosition true if the light pen triggered and a position could be sampled
// \param buttonsDown absolute state of the buttons (Button #0 -> 0, Button #1 -> 1, ...) 
void EventDriver_ReportLightPenDeviceChange(EventDriverRef _Nonnull pDriver, Int16 xAbs, Int16 yAbs, Bool hasPosition, UInt32 buttonsDown)
{
    const Int16 xDelta = (hasPosition) ? xAbs - pDriver->mouseX : pDriver->mouseX;
    const Int16 yDelta = (hasPosition) ? yAbs - pDriver->mouseY : pDriver->mouseY;
    
    EventDriver_ReportMouseDeviceChange(pDriver, xDelta, yDelta, buttonsDown);
}

// Reports a change in the state of a joystick device. Posts suitable events to
// the event queue.
// Must be called from the interrupt context with interrupts turned off.
// \param port the port number identifying the joystick
// \param xAbs current joystick X axis state (Int16.min -> 100% left, 0 -> resting, Int16.max -> 100% right)
// \param yAbs current joystick Y axis state (Int16.min -> 100% up, 0 -> resting, Int16.max -> 100% down)
// \param buttonsDown absolute state of the buttons (Button #0 -> 0, Button #1 -> 1, ...) 
void EventDriver_ReportJoystickDeviceChange(EventDriverRef _Nonnull pDriver, Int port, Int16 xAbs, Int16 yAbs, UInt32 buttonsDown)
{
    // Generate joystick button up/down events
    const UInt32 oldButtonsDown = pDriver->joystick[port].buttonsDown;

    if (buttonsDown != oldButtonsDown) {
        // XXX should be able to ask the joystick input driver how many buttons it supports
        for (Int i = 0; i < 2; i++) {
            const UInt32 old_down = oldButtonsDown & (1 << i);
            const UInt32 new_down = buttonsDown & (1 << i);
            HIDEventType evtType;
            HIDEventData evt;

            if (old_down ^ new_down) {
                if (old_down == 0 && new_down != 0) {
                    evtType = kHIDEventType_JoystickDown;
                } else {
                    evtType = kHIDEventType_JoystickUp;
                }

                evt.joystick.port = port;
                evt.joystick.buttonNumber = i;
                evt.joystick.flags = pDriver->modifierFlags;
                evt.joystick.direction.dx = xAbs;
                evt.joystick.direction.dy = yAbs;
                HIDEventQueue_Put(pDriver->eventQueue, evtType, &evt);
            }
        }
    }
    
    
    // Generate motion events
    const Int16 diffX = xAbs - pDriver->joystick[port].xAbs;
    const Int16 diffY = yAbs - pDriver->joystick[port].yAbs;
    
    if (diffX != 0 || diffY != 0) {
        HIDEventData evt;

        evt.joystickMotion.port = port;
        evt.joystickMotion.direction.dx = xAbs;
        evt.joystickMotion.direction.dy = yAbs;
        HIDEventQueue_Put(pDriver->eventQueue, kHIDEventType_JoystickMotion, &evt);
    }

    pDriver->joystick[port].xAbs = xAbs;
    pDriver->joystick[port].yAbs = yAbs;
    pDriver->joystick[port].buttonsDown = buttonsDown;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Kernel API
////////////////////////////////////////////////////////////////////////////////

// Creates a new input controller driver instance for the port 'portId'. Expects that the port
// is currently unassigned (aka type is == 'none').
ErrorCode EventDriver_CreateInputControllerForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, Int portId)
{
    decl_try_err();

    switch (type) {
        case kInputControllerType_None:
            break;
            
        case kInputControllerType_Mouse:
            try(MouseDriver_Create(pDriver, portId, &pDriver->port[portId].u.mouse.driver));
            break;
            
        case kInputControllerType_DigitalJoystick:
            try(DigitalJoystickDriver_Create(pDriver, portId, &pDriver->port[portId].u.digitalJoystick.driver));
            pDriver->joystick[portId].buttonsDown = 0;
            pDriver->joystick[portId].xAbs = 0;
            pDriver->joystick[portId].yAbs = 0;
            break;
            
        case kInputControllerType_AnalogJoystick:
            try(AnalogJoystickDriver_Create(pDriver, portId, &pDriver->port[portId].u.analogJoystick.driver));
            pDriver->joystick[portId].buttonsDown = 0;
            pDriver->joystick[portId].xAbs = 0;
            pDriver->joystick[portId].yAbs = 0;
            break;
            
        case kInputControllerType_LightPen:
            try(LightPenDriver_Create(pDriver, portId, &pDriver->port[portId].u.lightPen.driver));
            break;
            
        default:
            abort();
    }
    
    pDriver->port[portId].type = type;
    return EOK;

catch:
    return err;
}

// Destroys the input controller that is configured for port 'portId'. This frees the input controller
// specific driver and all associated state
void EventDriver_DestroyInputControllerForPort(EventDriverRef _Nonnull pDriver, Int portId)
{
    switch (pDriver->port[portId].type) {
        case kInputControllerType_None:
            break;
            
        case kInputControllerType_Mouse:
            MouseDriver_Destroy(pDriver->port[portId].u.mouse.driver);
            pDriver->port[portId].u.mouse.driver = NULL;
            break;
            
        case kInputControllerType_DigitalJoystick:
            DigitalJoystickDriver_Destroy(pDriver->port[portId].u.digitalJoystick.driver);
            pDriver->port[portId].u.digitalJoystick.driver = NULL;
            break;
            
        case kInputControllerType_AnalogJoystick:
            AnalogJoystickDriver_Destroy(pDriver->port[portId].u.analogJoystick.driver);
            pDriver->port[portId].u.analogJoystick.driver = NULL;
            break;
            
        case kInputControllerType_LightPen:
            LightPenDriver_Destroy(pDriver->port[portId].u.lightPen.driver);
            pDriver->port[portId].u.lightPen.driver = NULL;
            break;
            
        default:
            abort();
    }
    
    pDriver->port[portId].type = kInputControllerType_None;
}

InputControllerType EventDriver_GetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, Int portId)
{
    assert(portId >= 0 && portId < MAX_INPUT_CONTROLLER_PORTS);
    
    Lock_Lock(&pDriver->lock);
    const InputControllerType type = pDriver->port[portId].type;
    Lock_Unlock(&pDriver->lock);
    return type;
}

ErrorCode EventDriver_SetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, Int portId)
{
    decl_try_err();
    Bool needsUnlock = false;

    assert(portId >= 0 && portId < MAX_INPUT_CONTROLLER_PORTS);

    Lock_Lock(&pDriver->lock);
    needsUnlock = true;
    EventDriver_DestroyInputControllerForPort(pDriver, portId);
    try(EventDriver_CreateInputControllerForPort(pDriver, type, portId));
    Lock_Unlock(&pDriver->lock);
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&pDriver->lock);
    }
    return err;
}

void EventDriver_GetKeyRepeatDelays(EventDriverRef _Nonnull pDriver, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay)
{
    Lock_Lock(&pDriver->lock);
    KeyboardDriver_GetKeyRepeatDelays(pDriver->keyboardDriver, pInitialDelay, pRepeatDelay);
    Lock_Unlock(&pDriver->lock);
}

void EventDriver_SetKeyRepeatDelays(EventDriverRef _Nonnull pDriver, TimeInterval initialDelay, TimeInterval repeatDelay)
{
    Lock_Lock(&pDriver->lock);
    KeyboardDriver_SetKeyRepeatDelays(pDriver->keyboardDriver, initialDelay, repeatDelay);
    Lock_Unlock(&pDriver->lock);
}

static inline Bool KeyMap_IsKeyDown(const UInt32* _Nonnull pKeyMap, UInt16 keycode)
{
    assert(keycode <= 255);
    const UInt32 wordIdx = keycode >> 5;
    const UInt32 bitIdx = keycode - (wordIdx << 5);
    
    return (pKeyMap[wordIdx] & (1 << bitIdx)) != 0 ? true : false;
}

// Returns the keycodes of the keys that are currently pressed. All pressed keys
// are returned if 'nKeysToCheck' is 0. Only the keys which are pressed and in the
// set 'pKeysToCheck' are returned if 'nKeysToCheck' is > 0.
// This function returns the state of the keyboard hardware. This state is
// potentially (slightly) different from the state you get from inspecting the
// events in the event stream because the event stream lags the hardware state
// slightly.
void EventDriver_GetDeviceKeysDown(EventDriverRef _Nonnull pDriver, const HIDKeyCode* _Nullable pKeysToCheck, Int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, Int* _Nonnull nKeysDown)
{
    Int oi = 0;
    const Int irs = cpu_disable_irqs();
    
    if (nKeysToCheck > 0 && pKeysToCheck) {
        if (pKeysDown) {
            // Returns at most 'nKeysDown' keys which are in the set 'pKeysToCheck'
            for (Int i = 0; i < __min(nKeysToCheck, *nKeysDown); i++) {
                if (KeyMap_IsKeyDown(pDriver->keyMap, pKeysToCheck[i])) {
                    pKeysDown[oi++] = pKeysToCheck[i];
                }
            }
        } else {
            // Return the number of keys that are down and which are in the set 'pKeysToCheck'
            for (Int i = 0; i < nKeysToCheck; i++) {
                if (KeyMap_IsKeyDown(pDriver->keyMap, pKeysToCheck[i])) {
                    oi++;
                }
            }
        }
    } else {
        if (pKeysDown) {
            // Return all keys that are down
            for (Int i = 0; i < *nKeysDown; i++) {
                if (KeyMap_IsKeyDown(pDriver->keyMap, (HIDKeyCode)i)) {
                    pKeysDown[oi++] = (HIDKeyCode)i;
                }
            }
        } else {
            // Return the number of keys that are down
            for (Int i = 0; i < nKeysToCheck; i++) {
                if (KeyMap_IsKeyDown(pDriver->keyMap, (HIDKeyCode)i)) {
                    oi++;
                }
            }
        }
    }
    cpu_restore_irqs(irs);
    
    *nKeysDown = oi;
}

void EventDriver_SetMouseCursor(EventDriverRef _Nonnull pDriver, const Byte* pBitmap, const Byte* pMask)
{
    GraphicsDriver_SetMouseCursor(pDriver->graphicsDriver, pBitmap, pMask);
}

// Show the mouse cursor. This decrements the hidden counter. The mouse cursor
// is only shown if this counter reaches zero. The operation is carried out at
// the next vertical blank.
void EventDriver_ShowMouseCursor(EventDriverRef _Nonnull pDriver)
{
    Lock_Lock(&pDriver->lock);
    pDriver->mouseCursorHiddenCounter--;
    if (pDriver->mouseCursorHiddenCounter < 0) {
        pDriver->mouseCursorHiddenCounter = 0;
    }
    if (pDriver->mouseCursorHiddenCounter == 0) {
        GraphicsDriver_SetMouseCursorVisible(pDriver->graphicsDriver, true);
    }
    Lock_Unlock(&pDriver->lock);
}

// Hides the mouse cursor. This increments the hidden counter. The mouse remains
// hidden as long as the counter does not reach the value zero. The operation is
// carried out at the next vertical blank.
void EventDriver_HideMouseCursor(EventDriverRef _Nonnull pDriver)
{
    Lock_Lock(&pDriver->lock);
    if (pDriver->mouseCursorHiddenCounter == 0) {
        GraphicsDriver_SetMouseCursorVisible(pDriver->graphicsDriver, false);
    }
    pDriver->mouseCursorHiddenCounter++;
    Lock_Unlock(&pDriver->lock);
}

void EventDriver_SetMouseCursorHiddenUntilMouseMoves(EventDriverRef _Nonnull pDriver, Bool flag)
{
    GraphicsDriver_SetMouseCursorHiddenUntilMouseMoves(pDriver->graphicsDriver, flag);
}

// Returns the current mouse location in screen space.
Point EventDriver_GetMouseDevicePosition(EventDriverRef _Nonnull pDriver)
{
    Point loc;
    
    const Int irs = cpu_disable_irqs();
    loc.x = pDriver->mouseX;
    loc.y = pDriver->mouseY;
    cpu_restore_irqs(irs);
    return loc;
}

// Returns a bit mask of all the mouse buttons that are currently pressed.
UInt32 EventDriver_GetMouseDeviceButtonsDown(EventDriverRef _Nonnull pDriver)
{
    const Int irs = cpu_disable_irqs();
    const UInt32 buttons = pDriver->mouseButtons;
    cpu_restore_irqs(irs);
    return buttons;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Getting Events
////////////////////////////////////////////////////////////////////////////////

// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Blocks the caller if more events are requested
// than are queued.
ByteCount EventDriver_Read(EventDriverRef _Nonnull pDriver, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    decl_try_err();
    HIDEvent* pEvent = (HIDEvent*)pBuffer;
    ByteCount nBytesRead = 0;

    while ((nBytesRead + sizeof(HIDEvent)) <= nBytesToRead) {
        try(HIDEventQueue_Get(pDriver->eventQueue, pEvent, kTimeInterval_Infinity));
        //assert(HIDEventQueue_GetOverflowCount(pDriver->eventQueue) == 0);
        
        nBytesRead += sizeof(HIDEvent);
        pEvent++;
    }

    return nBytesRead;

catch:
    return -err;
}
