//
//  EventDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "EventDriverPriv.h"

//extern const uint16_t gArrow_Bits[];
//extern const uint16_t gArrow_Mask[];

// USB Keycode -> kHIDEventModifierFlag_XXX values which are be or'd / and'd into pDriver->modifierFlags.
// Bit 7 indicates whether the key is left or right: 0 -> left; 1 -> right
// shift        0x01
// option       0x02
// ctrl         0x04
// command      0x08
// caps lock    0x10
// keypad       0x20
// func         0x40
// isRight      0x80
static const uint8_t gUSBHIDKeyFlags[256] = {
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


errno_t EventDriver_Create(GraphicsDriverRef _Nonnull gdevice, EventDriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    EventDriver* pDriver;
    
    try(Object_Create(EventDriver, &pDriver));

    Lock_Init(&pDriver->lock);
    pDriver->gdevice = Object_RetainAs(gdevice, GraphicsDriver);

    pDriver->keyFlags = gUSBHIDKeyFlags;

    pDriver->screenLeft = 0;
    pDriver->screenTop = 0;
    pDriver->screenRight = (int16_t) GraphicsDriver_GetFramebufferSize(gdevice).width;
    pDriver->screenBottom = (int16_t) GraphicsDriver_GetFramebufferSize(gdevice).height;
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
    for (int i = 0; i < MAX_INPUT_CONTROLLER_PORTS; i++) {
        pDriver->port[i].type = kInputControllerType_None;
    }
    try(EventDriver_CreateInputControllerForPort(pDriver, kInputControllerType_Mouse, 0));


    // XXX
    //GraphicsDriver_SetMouseCursor(gdevice, gArrow_Bits, gArrow_Mask);
    //EventDriver_ShowMouseCursor(pDriver);
    // XXX

    *pOutDriver = pDriver;
    return EOK;
    
catch:
    Object_Release(pDriver);
    *pOutDriver = NULL;
    return err;
}

void EventDriver_deinit(EventDriverRef _Nonnull pDriver)
{
    for (int i = 0; i < MAX_INPUT_CONTROLLER_PORTS; i++) {
        EventDriver_DestroyInputControllerForPort(pDriver, i);
    }
    
    Object_Release(pDriver->keyboardDriver);
    pDriver->keyboardDriver = NULL;

    HIDEventQueue_Destroy(pDriver->eventQueue);

    Object_Release(pDriver->gdevice);
    pDriver->gdevice = NULL;

    Lock_Deinit(&pDriver->lock);
}

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Input Driver API
////////////////////////////////////////////////////////////////////////////////

GraphicsDriverRef _Nonnull EventDriver_GetGraphicsDriver(EventDriverRef _Nonnull pDriver)
{
    return pDriver->gdevice;
}

// Reports a key down, repeat or up from a keyboard device. This function updates
// the state of the logical keyboard and it posts a suitable keyboard event to
// the event queue.
// Must be called from the interrupt context with interrupts turned off.
void EventDriver_ReportKeyboardDeviceChange(EventDriverRef _Nonnull pDriver, HIDKeyState keyState, uint16_t keyCode)
{
    // Update the key map
    const uint32_t wordIdx = keyCode >> 5;
    const uint32_t bitIdx = keyCode - (wordIdx << 5);

    if (keyState == kHIDKeyState_Up) {
        pDriver->keyMap[wordIdx] &= ~(1 << bitIdx);
    } else {
        pDriver->keyMap[wordIdx] |= (1 << bitIdx);
    }


    // Update the modifier flags
    const uint32_t logModFlags = (keyCode <= 255) ? pDriver->keyFlags[keyCode] & 0x1f : 0;
    const bool isModifierKey = (logModFlags != 0);    
    uint32_t modifierFlags = pDriver->modifierFlags;

    if (isModifierKey) {
        const bool isRight = (keyCode <= 255) ? pDriver->keyFlags[keyCode] & 0x80 : 0;
        const uint32_t devModFlags = (isRight) ? logModFlags << 16 : logModFlags << 24;

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
    const uint32_t keyFunc = (keyCode <= 255) ? pDriver->keyFlags[keyCode] & 0x60 : 0;
    const uint32_t flags = modifierFlags | keyFunc;
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
// \param yDelta change in mouse position Y since last invocation
// \param buttonsDown absolute state of the mouse buttons (0 -> left button, 1 -> right button, 2-> middle button, ...) 
void EventDriver_ReportMouseDeviceChange(EventDriverRef _Nonnull pDriver, int16_t xDelta, int16_t yDelta, uint32_t buttonsDown)
{
    const uint32_t oldButtonsDown = pDriver->mouseButtons;
    const bool hasButtonsChange = (oldButtonsDown != buttonsDown);
    const bool hasPositionChange = (xDelta != 0 || yDelta != 0);

    if (hasPositionChange) {
        pDriver->mouseX += xDelta;
        pDriver->mouseY += yDelta;
        pDriver->mouseX = __min(__max(pDriver->mouseX, pDriver->screenLeft), pDriver->screenRight);
        pDriver->mouseY = __min(__max(pDriver->mouseY, pDriver->screenTop), pDriver->screenBottom);

        GraphicsDriver_SetMouseCursorPositionFromInterruptContext(pDriver->gdevice, pDriver->mouseX, pDriver->mouseY);
    }
    pDriver->mouseButtons = buttonsDown;


    if (hasButtonsChange) {
        HIDEventType evtType;
        HIDEventData evt;

        // Generate mouse button up/down events
        // XXX should be able to ask the mouse input driver how many buttons it supports
        for (int i = 0; i < 3; i++) {
            const uint32_t old_down = oldButtonsDown & (1 << i);
            const uint32_t new_down = buttonsDown & (1 << i);
            
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
void EventDriver_ReportLightPenDeviceChange(EventDriverRef _Nonnull pDriver, int16_t xAbs, int16_t yAbs, bool hasPosition, uint32_t buttonsDown)
{
    const int16_t xDelta = (hasPosition) ? xAbs - pDriver->mouseX : pDriver->mouseX;
    const int16_t yDelta = (hasPosition) ? yAbs - pDriver->mouseY : pDriver->mouseY;
    
    EventDriver_ReportMouseDeviceChange(pDriver, xDelta, yDelta, buttonsDown);
}

// Reports a change in the state of a joystick device. Posts suitable events to
// the event queue.
// Must be called from the interrupt context with interrupts turned off.
// \param port the port number identifying the joystick
// \param xAbs current joystick X axis state (int16_t.min -> 100% left, 0 -> resting, int16_t.max -> 100% right)
// \param yAbs current joystick Y axis state (int16_t.min -> 100% up, 0 -> resting, int16_t.max -> 100% down)
// \param buttonsDown absolute state of the buttons (Button #0 -> 0, Button #1 -> 1, ...) 
void EventDriver_ReportJoystickDeviceChange(EventDriverRef _Nonnull pDriver, int port, int16_t xAbs, int16_t yAbs, uint32_t buttonsDown)
{
    // Generate joystick button up/down events
    const uint32_t oldButtonsDown = pDriver->joystick[port].buttonsDown;

    if (buttonsDown != oldButtonsDown) {
        // XXX should be able to ask the joystick input driver how many buttons it supports
        for (int i = 0; i < 2; i++) {
            const uint32_t old_down = oldButtonsDown & (1 << i);
            const uint32_t new_down = buttonsDown & (1 << i);
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
    const int16_t diffX = xAbs - pDriver->joystick[port].xAbs;
    const int16_t diffY = yAbs - pDriver->joystick[port].yAbs;
    
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
errno_t EventDriver_CreateInputControllerForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, int portId)
{
    decl_try_err();

    switch (type) {
        case kInputControllerType_None:
            pDriver->port[portId].driver = NULL;
            break;
            
        case kInputControllerType_Mouse:
            try(MouseDriver_Create(pDriver, portId, (MouseDriverRef*)&pDriver->port[portId].driver));
            break;
            
        case kInputControllerType_DigitalJoystick:
            try(DigitalJoystickDriver_Create(pDriver, portId, (DigitalJoystickDriverRef*)&pDriver->port[portId].driver));
            pDriver->joystick[portId].buttonsDown = 0;
            pDriver->joystick[portId].xAbs = 0;
            pDriver->joystick[portId].yAbs = 0;
            break;
            
        case kInputControllerType_AnalogJoystick:
            try(AnalogJoystickDriver_Create(pDriver, portId, (AnalogJoystickDriverRef*)&pDriver->port[portId].driver));
            pDriver->joystick[portId].buttonsDown = 0;
            pDriver->joystick[portId].xAbs = 0;
            pDriver->joystick[portId].yAbs = 0;
            break;
            
        case kInputControllerType_LightPen:
            try(LightPenDriver_Create(pDriver, portId, (LightPenDriverRef*)&pDriver->port[portId].driver));
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
void EventDriver_DestroyInputControllerForPort(EventDriverRef _Nonnull pDriver, int portId)
{
    Object_Release(pDriver->port[portId].driver);
    pDriver->port[portId].driver = NULL;
    pDriver->port[portId].type = kInputControllerType_None;
}

InputControllerType EventDriver_GetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, int portId)
{
    assert(portId >= 0 && portId < MAX_INPUT_CONTROLLER_PORTS);
    
    Lock_Lock(&pDriver->lock);
    const InputControllerType type = pDriver->port[portId].type;
    Lock_Unlock(&pDriver->lock);
    return type;
}

errno_t EventDriver_SetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, int portId)
{
    decl_try_err();
    bool needsUnlock = false;

    if (portId < 0 || portId >= MAX_INPUT_CONTROLLER_PORTS) {
        throw(ENODEV);
    }

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

static inline bool KeyMap_IsKeyDown(const uint32_t* _Nonnull pKeyMap, uint16_t keycode)
{
    assert(keycode <= 255);
    const uint32_t wordIdx = keycode >> 5;
    const uint32_t bitIdx = keycode - (wordIdx << 5);
    
    return (pKeyMap[wordIdx] & (1 << bitIdx)) != 0 ? true : false;
}

// Returns the keycodes of the keys that are currently pressed. All pressed keys
// are returned if 'nKeysToCheck' is 0. Only the keys which are pressed and in the
// set 'pKeysToCheck' are returned if 'nKeysToCheck' is > 0.
// This function returns the state of the keyboard hardware. This state is
// potentially (slightly) different from the state you get from inspecting the
// events in the event stream because the event stream lags the hardware state
// slightly.
void EventDriver_GetDeviceKeysDown(EventDriverRef _Nonnull pDriver, const HIDKeyCode* _Nullable pKeysToCheck, int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, int* _Nonnull nKeysDown)
{
    int oi = 0;
    const int irs = cpu_disable_irqs();
    
    if (nKeysToCheck > 0 && pKeysToCheck) {
        if (pKeysDown) {
            // Returns at most 'nKeysDown' keys which are in the set 'pKeysToCheck'
            for (int i = 0; i < __min(nKeysToCheck, *nKeysDown); i++) {
                if (KeyMap_IsKeyDown(pDriver->keyMap, pKeysToCheck[i])) {
                    pKeysDown[oi++] = pKeysToCheck[i];
                }
            }
        } else {
            // Return the number of keys that are down and which are in the set 'pKeysToCheck'
            for (int i = 0; i < nKeysToCheck; i++) {
                if (KeyMap_IsKeyDown(pDriver->keyMap, pKeysToCheck[i])) {
                    oi++;
                }
            }
        }
    } else {
        if (pKeysDown) {
            // Return all keys that are down
            for (int i = 0; i < *nKeysDown; i++) {
                if (KeyMap_IsKeyDown(pDriver->keyMap, (HIDKeyCode)i)) {
                    pKeysDown[oi++] = (HIDKeyCode)i;
                }
            }
        } else {
            // Return the number of keys that are down
            for (int i = 0; i < nKeysToCheck; i++) {
                if (KeyMap_IsKeyDown(pDriver->keyMap, (HIDKeyCode)i)) {
                    oi++;
                }
            }
        }
    }
    cpu_restore_irqs(irs);
    
    *nKeysDown = oi;
}

void EventDriver_SetMouseCursor(EventDriverRef _Nonnull pDriver, const void* pBitmap, const void* pMask)
{
    GraphicsDriver_SetMouseCursor(pDriver->gdevice, pBitmap, pMask);
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
        GraphicsDriver_SetMouseCursorVisible(pDriver->gdevice, true);
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
        GraphicsDriver_SetMouseCursorVisible(pDriver->gdevice, false);
    }
    pDriver->mouseCursorHiddenCounter++;
    Lock_Unlock(&pDriver->lock);
}

void EventDriver_SetMouseCursorHiddenUntilMouseMoves(EventDriverRef _Nonnull pDriver, bool flag)
{
    GraphicsDriver_SetMouseCursorHiddenUntilMouseMoves(pDriver->gdevice, flag);
}

// Returns the current mouse location in screen space.
Point EventDriver_GetMouseDevicePosition(EventDriverRef _Nonnull pDriver)
{
    Point loc;
    
    const int irs = cpu_disable_irqs();
    loc.x = pDriver->mouseX;
    loc.y = pDriver->mouseY;
    cpu_restore_irqs(irs);
    return loc;
}

// Returns a bit mask of all the mouse buttons that are currently pressed.
uint32_t EventDriver_GetMouseDeviceButtonsDown(EventDriverRef _Nonnull pDriver)
{
    const int irs = cpu_disable_irqs();
    const uint32_t buttons = pDriver->mouseButtons;
    cpu_restore_irqs(irs);
    return buttons;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Getting Events
////////////////////////////////////////////////////////////////////////////////

// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
errno_t EventDriver_Read(EventDriverRef _Nonnull pDriver, void* _Nonnull pBuffer, ssize_t nBytesToRead, TimeInterval timeout, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    HIDEvent* pEvent = (HIDEvent*)pBuffer;
    ssize_t nBytesRead = 0;
    int i;

    while ((nBytesRead + sizeof(HIDEvent)) <= nBytesToRead) {
        const errno_t e1 = HIDEventQueue_Get(pDriver->eventQueue, pEvent, (i > 0) ? timeout : kTimeInterval_Zero);

        if (e1 != EOK) {
            // Return with an error if we were not able to read any event data at
            // all and return with the data we were able to read otherwise.
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        //assert(HIDEventQueue_GetOverflowCount(pDriver->eventQueue) == 0);
        
        nBytesRead += sizeof(HIDEvent);
        pEvent++;
        i++;
    }

    *nOutBytesRead = nBytesRead;
    return err;
}


class_func_defs(EventDriver, Object,
override_func_def(deinit, EventDriver, Object)
);
