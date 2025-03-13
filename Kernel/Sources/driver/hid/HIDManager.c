//
//  HIDManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "HIDManagerPriv.h"
#include <driver/DriverCatalog.h>
#include <driver/DriverChannel.h>

extern const uint8_t gUSBHIDKeyFlags[256];


HIDManagerRef _Nonnull gHIDManager;


errno_t HIDManager_Create(HIDManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    HIDManagerRef self;
    
    try(kalloc_cleared(sizeof(HIDManager), (void**)&self));

    Lock_Init(&self->lock);

    self->keyFlags = gUSBHIDKeyFlags;
    self->isMouseMoveReportingEnabled = false;
    self->mouseCursorVisibility = kMouseCursor_Hidden;


    // Create the HID event queue
    try(HIDEventQueue_Create(REPORT_QUEUE_MAX_EVENTS, &self->eventQueue));

    *pOutSelf = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t HIDManager_Start(HIDManagerRef _Nonnull self)
{
    decl_try_err();

    // Open a channel to the framebuffer
    try(DriverCatalog_OpenDriver(gDriverCatalog, kFramebufferName, kOpen_ReadWrite, &self->fbChannel));
    self->fb = DriverChannel_GetDriverAs(self->fbChannel, GraphicsDriver);

    int w, h;
    GraphicsDriver_GetDisplaySize(self->fb, &w, &h);

    self->screenLeft = 0;
    self->screenTop = 0;
    self->screenRight = (int16_t)w;
    self->screenBottom = (int16_t)h;


    // Open the keyboard driver
    try(DriverCatalog_OpenDriver(gDriverCatalog, "/kb", kOpen_ReadWrite, &self->kbChannel));


    // Open the game port driver
    try(DriverCatalog_OpenDriver(gDriverCatalog, "/gp-bus", kOpen_ReadWrite, &self->gpChannel));

catch:
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Input Driver API
////////////////////////////////////////////////////////////////////////////////

// Returns the current position of the light pen if the light pen triggered.
bool HIDManager_GetLightPenPosition(HIDManagerRef _Nonnull self, int16_t* _Nonnull pPosX, int16_t* _Nonnull pPosY)
{
    return GraphicsDriver_GetLightPenPositionFromInterruptContext(self->fb, pPosX, pPosY);
}

// Reports a key down, repeat or up from a keyboard device. This function updates
// the state of the logical keyboard and it posts a suitable keyboard event to
// the event queue.
// Must be called from the interrupt context with interrupts turned off.
void HIDManager_ReportKeyboardDeviceChange(HIDManagerRef _Nonnull self, HIDKeyState keyState, uint16_t keyCode)
{
    // Update the key map
    const uint32_t wordIdx = keyCode >> 5;
    const uint32_t bitIdx = keyCode - (wordIdx << 5);

    if (keyState == kHIDKeyState_Up) {
        self->keyMap[wordIdx] &= ~(1 << bitIdx);
    } else {
        self->keyMap[wordIdx] |= (1 << bitIdx);
    }


    // Update the modifier flags
    const uint32_t logModFlags = (keyCode <= 255) ? self->keyFlags[keyCode] & 0x1f : 0;
    const bool isModifierKey = (logModFlags != 0);    
    uint32_t modifierFlags = self->modifierFlags;

    if (isModifierKey) {
        const bool isRight = (keyCode <= 255) ? self->keyFlags[keyCode] & 0x80 : 0;
        const uint32_t devModFlags = (isRight) ? logModFlags << 16 : logModFlags << 24;

        if (keyState == kHIDKeyState_Up) {
            modifierFlags &= ~logModFlags;
            modifierFlags &= ~devModFlags;
        } else {
            modifierFlags |= logModFlags;
            modifierFlags |= devModFlags;
        }
        self->modifierFlags = modifierFlags;
    }


    // Generate and post the keyboard event
    const uint32_t keyFunc = (keyCode <= 255) ? self->keyFlags[keyCode] & 0x60 : 0;
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
    evt.key.isRepeat = (keyState == kHIDKeyState_Repeat) ? true : false;

    HIDEventQueue_Put(self->eventQueue, evtType, &evt);
}

// Reports a change in the state of a mouse device. Updates the state of the
// logical mouse device and posts suitable events to the event queue.
// Must be called from the interrupt context with interrupts turned off.
// \param xDelta change in mouse position X since last invocation
// \param yDelta change in mouse position Y since last invocation
// \param buttonsDown absolute state of the mouse buttons (0 -> left button, 1 -> right button, 2-> middle button, ...) 
void HIDManager_ReportMouseDeviceChange(HIDManagerRef _Nonnull self, int16_t xDelta, int16_t yDelta, uint32_t buttonsDown)
{
    const uint32_t oldButtonsDown = self->mouseButtons;
    const bool hasButtonsChange = (oldButtonsDown != buttonsDown);
    const bool hasPositionChange = (xDelta != 0 || yDelta != 0);

    if (hasPositionChange) {
        int mx = self->mouseX;
        int my = self->mouseY;

        mx += xDelta;
        my += yDelta;
        mx = __min(__max(mx, self->screenLeft), self->screenRight);
        my = __min(__max(my, self->screenTop), self->screenBottom);

        self->mouseX = mx;
        self->mouseY = my;
        mx -= self->hotSpotX;
        my -= self->hotSpotY;

        // Setting the new position will automatically make the mouse cursor visible
        // again if it was hidden-until-move. The reason is that the hide-until-move
        // function simply sets the mouse cursor to a position outside the visible
        // scree area to (temporarily) hide it.
        if (self->mouseCursorVisibility != kMouseCursor_Hidden) {
            if (self->isMouseShieldActive
                && mx >= self->shieldingLeft && mx < self->shieldingRight && my >= self->shieldingTop && my < self->shieldingBottom) {
                GraphicsDriver_SetMouseCursorPositionFromInterruptContext(self->fb, INT_MIN, INT_MIN);
            }
            else {
                GraphicsDriver_SetMouseCursorPositionFromInterruptContext(self->fb, mx, my);
                if (self->mouseCursorVisibility == kMouseCursor_HiddenUntilMove) {
                    self->mouseCursorVisibility = kMouseCursor_Visible;
                }
            }
        }
    }
    self->mouseButtons = buttonsDown;


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
                evt.mouse.flags = self->modifierFlags;
                evt.mouse.x = self->mouseX;
                evt.mouse.y = self->mouseY;
                HIDEventQueue_Put(self->eventQueue, evtType, &evt);
            }
        }
    }
    else if (hasPositionChange && self->isMouseMoveReportingEnabled) {
        HIDEventData evt;

        evt.mouseMoved.flags = self->modifierFlags;
        evt.mouseMoved.x = self->mouseX;
        evt.mouseMoved.y = self->mouseY;
        HIDEventQueue_Put(self->eventQueue, kHIDEventType_MouseMoved, &evt);
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
void HIDManager_ReportLightPenDeviceChange(HIDManagerRef _Nonnull self, int16_t xAbs, int16_t yAbs, bool hasPosition, uint32_t buttonsDown)
{
    const int16_t xDelta = (hasPosition) ? xAbs - self->mouseX : self->mouseX;
    const int16_t yDelta = (hasPosition) ? yAbs - self->mouseY : self->mouseY;
    
    HIDManager_ReportMouseDeviceChange(self, xDelta, yDelta, buttonsDown);
}

// Reports a change in the state of a joystick device. Posts suitable events to
// the event queue.
// Must be called from the interrupt context with interrupts turned off.
// \param port the port number identifying the joystick
// \param xAbs current joystick X axis state (int16_t.min -> 100% left, 0 -> resting, int16_t.max -> 100% right)
// \param yAbs current joystick Y axis state (int16_t.min -> 100% up, 0 -> resting, int16_t.max -> 100% down)
// \param buttonsDown absolute state of the buttons (Button #0 -> 0, Button #1 -> 1, ...) 
void HIDManager_ReportJoystickDeviceChange(HIDManagerRef _Nonnull self, int port, int16_t xAbs, int16_t yAbs, uint32_t buttonsDown)
{
    // Generate joystick button up/down events
    const uint32_t oldButtonsDown = self->joystick[port].buttonsDown;

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
                evt.joystick.flags = self->modifierFlags;
                evt.joystick.dx = xAbs;
                evt.joystick.dy = yAbs;
                HIDEventQueue_Put(self->eventQueue, evtType, &evt);
            }
        }
    }
    
    
    // Generate motion events
    const int16_t diffX = xAbs - self->joystick[port].xAbs;
    const int16_t diffY = yAbs - self->joystick[port].yAbs;
    
    if (diffX != 0 || diffY != 0) {
        HIDEventData evt;

        evt.joystickMotion.port = port;
        evt.joystickMotion.dx = xAbs;
        evt.joystickMotion.dy = yAbs;
        HIDEventQueue_Put(self->eventQueue, kHIDEventType_JoystickMotion, &evt);
    }

    self->joystick[port].xAbs = xAbs;
    self->joystick[port].yAbs = yAbs;
    self->joystick[port].buttonsDown = buttonsDown;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Kernel API
////////////////////////////////////////////////////////////////////////////////

void HIDManager_GetKeyRepeatDelays(HIDManagerRef _Nonnull self, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay)
{
    Lock_Lock(&self->lock);
    IOChannel_Ioctl(self->kbChannel, kKeyboardCommand_GetKeyRepeatDelays, pInitialDelay, pRepeatDelay);
    Lock_Unlock(&self->lock);
}

void HIDManager_SetKeyRepeatDelays(HIDManagerRef _Nonnull self, TimeInterval initialDelay, TimeInterval repeatDelay)
{
    Lock_Lock(&self->lock);
    IOChannel_Ioctl(self->kbChannel, kKeyboardCommand_SetKeyRepeatDelays, initialDelay, repeatDelay);
    Lock_Unlock(&self->lock);
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
void HIDManager_GetDeviceKeysDown(HIDManagerRef _Nonnull self, const HIDKeyCode* _Nullable pKeysToCheck, int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, int* _Nonnull nKeysDown)
{
    int oi = 0;
    const int irs = cpu_disable_irqs();
    
    if (nKeysToCheck > 0 && pKeysToCheck) {
        if (pKeysDown) {
            // Returns at most 'nKeysDown' keys which are in the set 'pKeysToCheck'
            for (int i = 0; i < __min(nKeysToCheck, *nKeysDown); i++) {
                if (KeyMap_IsKeyDown(self->keyMap, pKeysToCheck[i])) {
                    pKeysDown[oi++] = pKeysToCheck[i];
                }
            }
        } else {
            // Return the number of keys that are down and which are in the set 'pKeysToCheck'
            for (int i = 0; i < nKeysToCheck; i++) {
                if (KeyMap_IsKeyDown(self->keyMap, pKeysToCheck[i])) {
                    oi++;
                }
            }
        }
    } else {
        if (pKeysDown) {
            // Return all keys that are down
            for (int i = 0; i < *nKeysDown; i++) {
                if (KeyMap_IsKeyDown(self->keyMap, (HIDKeyCode)i)) {
                    pKeysDown[oi++] = (HIDKeyCode)i;
                }
            }
        } else {
            // Return the number of keys that are down
            for (int i = 0; i < nKeysToCheck; i++) {
                if (KeyMap_IsKeyDown(self->keyMap, (HIDKeyCode)i)) {
                    oi++;
                }
            }
        }
    }
    cpu_restore_irqs(irs);
    
    *nKeysDown = oi;
}

errno_t HIDManager_GetPortDevice(HIDManagerRef _Nonnull self, int port, InputType* _Nullable pOutType)
{
    Lock_Lock(&self->lock);
    const errno_t err = IOChannel_Ioctl(self->gpChannel, kGamePortCommand_GetPortDevice, port, pOutType);
    Lock_Unlock(&self->lock);
    return err;
}

errno_t HIDManager_SetPortDevice(HIDManagerRef _Nonnull self, int port, InputType type)
{
    Lock_Lock(&self->lock);
    const errno_t err = IOChannel_Ioctl(self->gpChannel, kGamePortCommand_SetPortDevice, port, type);
    Lock_Unlock(&self->lock);
    return err;
}

errno_t HIDManager_SetMouseCursor(HIDManagerRef _Nonnull self, const uint16_t* _Nullable planes[2], int width, int height, PixelFormat pixelFormat, int hotSpotX, int hotSpotY)
{
    Lock_Lock(&self->lock);
    const errno_t err = GraphicsDriver_SetMouseCursor(self->fb, planes, width, height, pixelFormat);
    if (err == EOK) {
        self->hotSpotX = __max(__min(hotSpotX, INT16_MAX), INT16_MIN);
        self->hotSpotY = __max(__min(hotSpotY, INT16_MAX), INT16_MIN);
    }
    Lock_Unlock(&self->lock);
    return err;
}

// Changes the mouse cursor visibility to visible, hidden altogether or hidden
// until the user moves the mouse cursor. Note that the visibility state is
// absolute - nesting of calls isn't supported in this sense.
// set_mouse_cursor_visibility(MouseCursorVisibility mode)
errno_t HIDManager_SetMouseCursorVisibility(HIDManagerRef _Nonnull self, MouseCursorVisibility mode)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    self->mouseCursorVisibility = mode;
    switch (mode) {
        case kMouseCursor_Hidden:
            GraphicsDriver_SetMouseCursorPosition(self->fb, INT_MIN, INT_MIN);
            break;

        case kMouseCursor_HiddenUntilMove:
            // Temporarily hide the mouse cursor. The next mouse report with an
            // actual move will cause the mouse cursor to become visible again
            // because it will be set to a location inside the visible screen
            // bounds. 
            GraphicsDriver_SetMouseCursorPosition(self->fb, INT_MIN, INT_MIN);
            break;

        case kMouseCursor_Visible:
            GraphicsDriver_SetMouseCursorPosition(self->fb, self->mouseX, self->mouseY);
            break;

        default:
            err = EINVAL;
            break;
    }
    Lock_Unlock(&self->lock);
    return err;
}

MouseCursorVisibility HIDManager_GetMouseCursorVisibility(HIDManagerRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const MouseCursorVisibility mode = self->mouseCursorVisibility;
    Lock_Unlock(&self->lock);

    return mode;
}

errno_t HIDManager_ShieldMouseCursor(HIDManagerRef _Nonnull self, int x, int y, int width, int height)
{
    if (width < 0 || height < 0) {
        return EINVAL;
    }

    Lock_Lock(&self->lock);

    int l = x;
    int r = x + width;
    int t = y;
    int b = y + height;

    self->isMouseShieldActive = (width > 0 && height > 0) ? true : false;
    if (self->isMouseShieldActive) {
        r += kMouseCursor_Width;
        b += kMouseCursor_Height;
    }

    l = __max(__min(l, INT16_MAX), INT16_MIN);
    r = __max(__min(r, INT16_MAX), INT16_MIN);
    t = __max(__min(t, INT16_MAX), INT16_MIN);
    b = __max(__min(b, INT16_MAX), INT16_MIN);

    self->shieldingLeft = l;
    self->shieldingTop = t;
    self->shieldingRight = r;
    self->shieldingBottom = b;

    const int mx = self->mouseX - self->hotSpotX;
    const int my = self->mouseY - self->mouseY;
    if (mx >= l && mx < r && my >= t && my < b) {
        GraphicsDriver_SetMouseCursorPosition(self->fb, INT_MIN, INT_MIN);
    }

    Lock_Unlock(&self->lock);
}

void HIDManager_UnshieldMouseCursor(HIDManagerRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    self->isMouseShieldActive = false;
    
    if (self->mouseCursorVisibility == kMouseCursor_Visible) {
        GraphicsDriver_SetMouseCursorPosition(self->fb, self->mouseX, self->mouseY);
    }
    Lock_Unlock(&self->lock);
}

// Returns the current mouse location in screen space.
void HIDManager_GetMouseDevicePosition(HIDManagerRef _Nonnull self, int* _Nonnull pOutX, int* _Nonnull pOutY)
{
    const int irs = cpu_disable_irqs();
    *pOutX = self->mouseX;
    *pOutY = self->mouseY;
    cpu_restore_irqs(irs);
}

// Returns a bit mask of all the mouse buttons that are currently pressed.
uint32_t HIDManager_GetMouseDeviceButtonsDown(HIDManagerRef _Nonnull self)
{
    const int irs = cpu_disable_irqs();
    const uint32_t buttons = self->mouseButtons;
    cpu_restore_irqs(irs);
    return buttons;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Getting Events
////////////////////////////////////////////////////////////////////////////////

// Dequeues and returns the next available event or ETIMEOUT if no event is
// available.
errno_t HIDManager_GetNextEvent(HIDManagerRef _Nonnull self, TimeInterval timeout, HIDEvent* _Nonnull evt)
{
    return HIDEventQueue_Get(self->eventQueue, timeout, evt);
}
