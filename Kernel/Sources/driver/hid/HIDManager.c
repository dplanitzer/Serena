//
//  HIDManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "HIDManagerPriv.h"
#include <driver/DriverManager.h>
#include <handler/HandlerChannel.h>
#include <kern/kalloc.h>
#include <kern/limits.h>
#include <kpi/fcntl.h>
#include <process/Process.h>


extern const uint8_t gUSBHIDKeyFlags[256];
int _vbl_handler(HIDManagerRef _Nonnull self);
static void _reports_collector_loop(HIDManagerRef _Nonnull self);


HIDManagerRef _Nonnull gHIDManager;


errno_t HIDManager_Create(HIDManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    HIDManagerRef self;
    
    try(kalloc_cleared(sizeof(HIDManager), (void**)&self));

    mtx_init(&self->mtx);
    wq_init(&self->reportsWaitQueue);

    self->reportSigs = _SIGBIT(SIGKEY) | _SIGBIT(SIGVBL);
    self->report.type = kHIDReportType_Null;

    self->keyFlags = gUSBHIDKeyFlags;
    self->isMouseMoveReportingEnabled = false;
    self->mouse.visibility = kMouseCursor_Hidden;


    // Create the HID event queue
    try(HIDEventQueue_Create(REPORT_QUEUE_MAX_EVENTS, &self->eventQueue));

    self->vblHandler.id = IRQ_ID_VBLANK;
    self->vblHandler.priority = IRQ_PRI_HIGHEST + 10;
    self->vblHandler.enabled = true;
    self->vblHandler.func = (irq_handler_func_t)_vbl_handler;
    self->vblHandler.arg = self;


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

    // Create the event vcpu
    _vcpu_acquire_attr_t attr;
    attr.func = (vcpu_func_t)_reports_collector_loop;
    attr.arg = self;
    attr.stack_size = 0;
    attr.groupid = VCPUID_MAIN_GROUP;
    attr.sched_params.qos = VCPU_QOS_REALTIME;
    attr.sched_params.priority = VCPU_PRI_HIGHEST - 1;
    attr.flags = VCPU_ACQUIRE_RESUMED;
    attr.data = 0;
    try(Process_AcquireVirtualProcessor(gKernelProcess, &attr, &self->reportsCollector));


    // Enable VBL interrupts
    irq_add_handler(&self->vblHandler);

catch:
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Kernel API
////////////////////////////////////////////////////////////////////////////////

void HIDManager_GetKeyRepeatDelays(HIDManagerRef _Nonnull self, struct timespec* _Nullable pInitialDelay, struct timespec* _Nullable pRepeatDelay)
{
    mtx_lock(&self->mtx);
    HIDEventQueue_GetKeyRepeatDelays(self->eventQueue, pInitialDelay, pRepeatDelay);
    mtx_unlock(&self->mtx);
}

void HIDManager_SetKeyRepeatDelays(HIDManagerRef _Nonnull self, const struct timespec* _Nonnull initialDelay, const struct timespec* _Nonnull repeatDelay)
{
    mtx_lock(&self->mtx);
    HIDEventQueue_SetKeyRepeatDelays(self->eventQueue, initialDelay, repeatDelay);
    mtx_unlock(&self->mtx);
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

    mtx_lock(&self->mtx);
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
    mtx_unlock(&self->mtx);
    
    *nKeysDown = oi;
}

errno_t HIDManager_SetMouseCursor(HIDManagerRef _Nonnull self, const uint16_t* _Nullable planes[2], int width, int height, PixelFormat pixelFormat, int hotSpotX, int hotSpotY)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    if (self->fb) {
        err = GraphicsDriver_SetMouseCursor(self->fb, planes, width, height, pixelFormat);
        
        if (err == EOK) {
            self->hotSpotX = __max(__min(hotSpotX, INT16_MAX), INT16_MIN);
            self->hotSpotY = __max(__min(hotSpotY, INT16_MAX), INT16_MIN);
        }
    }
    mtx_unlock(&self->mtx);
    return err;
}

// Changes the mouse cursor visibility to visible, hidden altogether or hidden
// until the user moves the mouse cursor. Note that the visibility state is
// absolute - nesting of calls isn't supported in this sense.
// set_mouse_cursor_visibility(MouseCursorVisibility mode)
errno_t HIDManager_SetMouseCursorVisibility(HIDManagerRef _Nonnull self, MouseCursorVisibility mode)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    self->mouse.visibility = mode;
    if (self->fb) {
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
            GraphicsDriver_SetMouseCursorPosition(self->fb, self->mouse.x, self->mouse.y);
            break;

        default:
            err = EINVAL;
            break;
        }
    }
    mtx_unlock(&self->mtx);
    return err;
}

MouseCursorVisibility HIDManager_GetMouseCursorVisibility(HIDManagerRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const MouseCursorVisibility mode = self->mouse.visibility;
    mtx_unlock(&self->mtx);

    return mode;
}

errno_t HIDManager_ShieldMouseCursor(HIDManagerRef _Nonnull self, int x, int y, int width, int height)
{
    if (width < 0 || height < 0) {
        return EINVAL;
    }

    mtx_lock(&self->mtx);

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

    const int mx = self->mouse.x - self->hotSpotX;
    const int my = self->mouse.y - self->mouse.y;
    if (mx >= l && mx < r && my >= t && my < b && self->fb) {
        GraphicsDriver_SetMouseCursorPosition(self->fb, INT_MIN, INT_MIN);
    }

    mtx_unlock(&self->mtx);
}

void HIDManager_UnshieldMouseCursor(HIDManagerRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    self->isMouseShieldActive = false;
    
    if (self->mouse.visibility == kMouseCursor_Visible && self->fb) {
        GraphicsDriver_SetMouseCursorPosition(self->fb, self->mouse.x, self->mouse.y);
    }
    mtx_unlock(&self->mtx);
}

// Returns the current mouse location in screen space.
void HIDManager_GetMouseDevicePosition(HIDManagerRef _Nonnull self, int* _Nonnull pOutX, int* _Nonnull pOutY)
{
    mtx_lock(&self->mtx);
    *pOutX = self->mouse.x;
    *pOutY = self->mouse.y;
    mtx_unlock(&self->mtx);
}

// Returns a bit mask of all the mouse buttons that are currently pressed.
uint32_t HIDManager_GetMouseDeviceButtonsDown(HIDManagerRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const uint32_t buttons = self->mouse.buttons;
    mtx_unlock(&self->mtx);

    return buttons;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Getting Events
////////////////////////////////////////////////////////////////////////////////

// Dequeues and returns the next available event or ETIMEDOUT if no event is
// available and a timeout > 0 was specified. Returns EAGAIN if no event is
// available and the timeout is 0.
errno_t HIDManager_GetNextEvent(HIDManagerRef _Nonnull self, const struct timespec* _Nonnull timeout, HIDEvent* _Nonnull evt)
{
    return HIDEventQueue_Get(self->eventQueue, timeout, evt);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Event Generation and Posting
////////////////////////////////////////////////////////////////////////////////

// Reports a key down or up from a keyboard device. This function updates the
// state of the logical keyboard and it posts a suitable keyboard event to the
// event queue.
static void _post_key_event(HIDManagerRef _Nonnull _Locked self, const HIDReport* _Nonnull report)
{
    // Update the key map
    const uint16_t keyCode = report->data.key.keyCode;
    const uint32_t wordIdx = keyCode >> 5;
    const uint32_t bitIdx = keyCode - (wordIdx << 5);

    if (report->type == kHIDReportType_KeyUp) {
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

        if (report->type == kHIDReportType_KeyUp) {
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
        evtType = (report->type == kHIDReportType_KeyUp) ? kHIDEventType_KeyUp : kHIDEventType_KeyDown;
    } else {
        evtType = kHIDEventType_FlagsChanged;
    }
    evt.key.flags = flags;
    evt.key.keyCode = keyCode;
    evt.key.isRepeat = false;

    HIDEventQueue_Put(self->eventQueue, evtType, &evt);
}

// Posts suitable mouse events to the event queue.
static void _post_mouse_event(HIDManagerRef _Nonnull _Locked self, bool hasPositionChange, bool hasButtonsChange, uint32_t oldButtonsDown)
{
    if (hasPositionChange) {
        const int mx = self->mouse.x - self->hotSpotX;
        const int my = self->mouse.y - self->hotSpotY;

        // Setting the new position will automatically make the mouse cursor visible
        // again if it was hidden-until-move. The reason is that the hide-until-move
        // function simply sets the mouse cursor to a position outside the visible
        // scree area to (temporarily) hide it.
        if (self->mouse.visibility != kMouseCursor_Hidden && self->fb) {
            if (self->isMouseShieldActive
                && mx >= self->shieldingLeft && mx < self->shieldingRight && my >= self->shieldingTop && my < self->shieldingBottom) {
                GraphicsDriver_SetMouseCursorPosition(self->fb, INT_MIN, INT_MIN);
            }
            else {
                GraphicsDriver_SetMouseCursorPosition(self->fb, mx, my);
                if (self->mouse.visibility == kMouseCursor_HiddenUntilMove) {
                    self->mouse.visibility = kMouseCursor_Visible;
                }
            }
        }
    }


    if (hasButtonsChange) {
        HIDEventType evtType;
        HIDEventData evt;

        // Generate mouse button up/down events
        // XXX should be able to ask the mouse input driver how many buttons it supports
        for (int i = 0; i < 3; i++) {
            const uint32_t old_down = oldButtonsDown & (1 << i);
            const uint32_t new_down = self->mouse.buttons & (1 << i);
            
            if (old_down ^ new_down) {
                if (old_down == 0 && new_down != 0) {
                    evtType = kHIDEventType_MouseDown;
                } else {
                    evtType = kHIDEventType_MouseUp;
                }
                evt.mouse.buttonNumber = i;
                evt.mouse.flags = self->modifierFlags;
                evt.mouse.x = self->mouse.x;
                evt.mouse.y = self->mouse.y;
                HIDEventQueue_Put(self->eventQueue, evtType, &evt);
            }
        }
    }
    else if (hasPositionChange && self->isMouseMoveReportingEnabled) {
        HIDEventData evt;

        evt.mouseMoved.flags = self->modifierFlags;
        evt.mouseMoved.x = self->mouse.x;
        evt.mouseMoved.y = self->mouse.y;
        HIDEventQueue_Put(self->eventQueue, kHIDEventType_MouseMoved, &evt);
    }
}


// Reports a change in the state of a light pen device. Posts suitable events to
// the event queue. The light pen controls the mouse cursor and generates mouse
// events.
#if 0
static void _post_lp_event(HIDManagerRef _Nonnull _Locked self, const HIDReport* _Nonnull report)
{
    const int16_t dx = (report->data.lp.hasPosition) ? report->data.lp.x - self->mouseX : self->mouseX;
    const int16_t dy = (report->data.lp.hasPosition) ? report->data.lp.y - self->mouseY : self->mouseY;
    const uint32_t buttons = report->data.lp.buttons;
    
    self->report.type = kHIDReportType_Mouse;
    self->report.data.mouse.dx = dx;
    self->report.data.mouse.dy = dy;
    self->report.data.mouse.buttons = buttons;
    _post_mouse_event(self, &self->report);
}
#endif

// Reports a change in the state of a gamepad style device. Posts suitable events
// to the event queue.
static void _post_gamepad_event(HIDManagerRef _Nonnull _Locked self, gamepad_state_t* _Nonnull gp, const HIDReport* _Nonnull report)
{
    // Generate button up/down events
    const uint32_t oldButtons = gp->buttons;

    if (report->data.joy.buttons != oldButtons) {
        // XXX should be able to ask the joystick input driver how many buttons it supports
        for (int i = 0; i < 2; i++) {
            const uint32_t old_down = oldButtons & (1 << i);
            const uint32_t new_down = report->data.joy.buttons & (1 << i);
            HIDEventType evtType;
            HIDEventData evt;

            if (old_down ^ new_down) {
                if (old_down == 0 && new_down != 0) {
                    evtType = kHIDEventType_JoystickDown;
                } else {
                    evtType = kHIDEventType_JoystickUp;
                }

                evt.joystick.port = 0; //XXX driver id
                evt.joystick.buttonNumber = i;
                evt.joystick.flags = self->modifierFlags;
                evt.joystick.dx = report->data.joy.x;
                evt.joystick.dy = report->data.joy.y;
                HIDEventQueue_Put(self->eventQueue, evtType, &evt);
            }
        }
    }
    
    
    // Generate motion events
    const int16_t diffX = report->data.joy.x - gp->x;
    const int16_t diffY = report->data.joy.y - gp->y;
    
    if (diffX != 0 || diffY != 0) {
        HIDEventData evt;

        evt.joystickMotion.port = 0;    //XXX driver id
        evt.joystickMotion.dx = report->data.joy.x;
        evt.joystickMotion.dy = report->data.joy.y;
        HIDEventQueue_Put(self->eventQueue, kHIDEventType_JoystickMotion, &evt);
    }

    gp->x = report->data.joy.x;
    gp->y = report->data.joy.y;
    gp->buttons = report->data.joy.buttons;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: HID Reports Collector
////////////////////////////////////////////////////////////////////////////////

IOCATS_DEF(g_gamepad_cats, IOHID_ANALOG_JOYSTICK, IOHID_DIGITAL_JOYSTICK);

// Connects the given HID input driver or framebuffer to the HID manager by
// opening a channel to it.
static void _connect_driver(HIDManagerRef _Nonnull _Locked self, DriverRef _Nonnull driver)
{
    decl_try_err();

    if (self->kbChannel == NULL && Driver_MatchesCategory(driver, IOHID_KEYBOARD)) {
        err = Driver_Open(driver, O_RDWR, 0, &self->kbChannel);
        if (err == EOK) {
            InputDriver_SetReportTarget(driver, self->reportsCollector, SIGKEY);
            self->kb = (InputDriverRef)driver;
        }
    }
    else if (self->mouse.chCount < MAX_POINTING_DEVICES && Driver_MatchesCategory(driver, IOHID_MOUSE)) {
        for (int i = 0; i < MAX_POINTING_DEVICES; i++) {
            if (self->mouse.ch[i] == NULL) {
                err = Driver_Open(driver, O_RDWR, 0, &self->mouse.ch[i]);
                if (err == EOK) {
                    self->mouse.chCount++;
                }
                break;
            }
        }
    }
    else if (self->gamepadCount < MAX_GAME_PADS && Driver_MatchesAnyCategory(driver, g_gamepad_cats)) {
        for (int i = 0; i < MAX_GAME_PADS; i++) {
            gamepad_state_t* gp = &self->gamepad[i];

            if (gp->ch == NULL) {
                err = Driver_Open(driver, O_RDWR, 0, &gp->ch);
                if (err == EOK) {
                    gp->x = 0;
                    gp->y = 0;
                    gp->buttons = 0;
                    self->gamepadCount++;
                }
                break;
            }
        }
    }
    else if (self->fbChannel == NULL && Driver_MatchesCategory(driver, IOVID_FB)) {
        // Open a channel to the framebuffer
        err = Driver_Open(driver, O_RDWR, 0, &self->fbChannel);
        if (err == EOK) {
            self->fb = HandlerChannel_GetHandlerAs(self->fbChannel, GraphicsDriver);

            int w, h;
            GraphicsDriver_GetDisplaySize(self->fb, &w, &h);

            self->screenLeft = 0;
            self->screenTop = 0;
            self->screenRight = (int16_t)w;
            self->screenBottom = (int16_t)h;
        }
    }
}

// Disconnects the given HID driver or framebuffer.
static void _disconnect_driver(HIDManagerRef _Nonnull _Locked self, DriverRef _Nonnull driver)
{
    if ((DriverRef)self->kb == driver) {
        IOChannel_Release(self->kbChannel);
        self->kbChannel = NULL;
        self->kb = NULL;
        return;
    }
    
    if ((DriverRef)self->fb == driver) {
        IOChannel_Release(self->fbChannel);
        self->fbChannel = NULL;
        self->fb = NULL;
        self->screenRight = 0;
        self->screenBottom = 0;
        return;
    }

    for (int i = 0; i < MAX_POINTING_DEVICES; i++) {
        IOChannelRef ch = self->mouse.ch[i];

        if (ch && HandlerChannel_GetHandlerAs(ch, Driver) == driver) {
            IOChannel_Release(ch);
            self->mouse.ch[i] = NULL;
            self->mouse.chCount--;
            break;
        }
    }

    for (int i = 0; i < MAX_GAME_PADS; i++) {
        gamepad_state_t* gp = &self->gamepad[i];

        if (gp->ch && HandlerChannel_GetHandlerAs(gp->ch, Driver) == driver) {
            IOChannel_Release(gp->ch);
            gp->ch = NULL;
            self->gamepadCount--;
            break;
        }
    }
}

static void _matching_driver(HIDManagerRef _Nonnull self, DriverRef _Nonnull driver, int action)
{
    mtx_lock(&self->mtx);

    switch (action) {
        case IOACTION_PUBLISH:
            _connect_driver(self, driver);
            break;

        case IOACTION_UNPUBLISH:
            _disconnect_driver(self, driver);
            break;

        default:
            break;
    }

    mtx_unlock(&self->mtx);
}


int _vbl_handler(HIDManagerRef _Nonnull self)
{
    vcpu_sigsend_irq(self->reportsCollector, SIGVBL, false);
    return 0;
}


static void _collect_keyboard_reports(HIDManagerRef _Nonnull self)
{
    for (;;) {
        InputDriver_GetReport(self->kb, &self->report);
                
        if (self->report.type == kHIDReportType_Null) {
            break;
        }

        _post_key_event(self, &self->report);
    }
}

static void _collect_pointing_device_reports(HIDManagerRef _Nonnull self)
{
    if (self->mouse.chCount == 0) {
        return;
    }

    const int16_t oldX = self->mouse.x;
    const int16_t oldY = self->mouse.y;
    const uint32_t oldButtonsDown = self->mouse.buttons;

    for (int i = 0; i < self->mouse.chCount; i++) {
        IOChannelRef ch = self->mouse.ch[i];

        if (ch) {
            InputDriver_GetReport(HandlerChannel_GetHandlerAs(ch, InputDriver), &self->report);

            if (self->report.data.mouse.dx != 0 || self->report.data.mouse.dy != 0) {
                int mx = self->mouse.x;
                int my = self->mouse.y;

                mx += self->report.data.mouse.dx;
                my += self->report.data.mouse.dy;
                mx = __min(__max(mx, self->screenLeft), self->screenRight);
                my = __min(__max(my, self->screenTop), self->screenBottom);

                self->mouse.x = mx;
                self->mouse.y = my;
            }
            if (self->mouse.buttons != self->report.data.mouse.buttons) {
                self->mouse.buttons = self->report.data.mouse.buttons;
            }
        }
    }

    const bool hasButtonsChange = (oldButtonsDown != self->report.data.mouse.buttons);
    const bool hasPositionChange = (self->report.data.mouse.dx != 0 || self->report.data.mouse.dy != 0);

    _post_mouse_event(self, hasPositionChange, hasButtonsChange, oldButtonsDown);
}

static void _collect_gamepad_reports(HIDManagerRef _Nonnull self)
{
    for (int i = 0; i < self->gamepadCount; i++) {
        gamepad_state_t* gp = &self->gamepad[i];

        if (gp->ch) {
            InputDriver_GetReport(HandlerChannel_GetHandlerAs(gp->ch, InputDriver), &self->report);
            _post_gamepad_event(self, gp, &self->report);
        }
    }
}


IOCATS_DEF(g_hid_cats, IOHID_KEYBOARD, IOHID_KEYPAD, IOHID_MOUSE, IOHID_LIGHTPEN,
    IOHID_STYLUS, IOHID_TRACKBALL, IOHID_ANALOG_JOYSTICK, IOHID_DIGITAL_JOYSTICK,
    IOVID_FB);

static void _reports_collector_loop(HIDManagerRef _Nonnull self)
{
    int signo = 0;

    DriverManager_StartMatching(gDriverManager, g_hid_cats, (drv_match_func_t)_matching_driver, self);

    mtx_lock(&self->mtx);

    for (;;) {
        self->report.type = kHIDReportType_Null;

        switch (signo) {
            case SIGKEY:
                _collect_keyboard_reports(self);
                break;

            case SIGVBL:
                _collect_pointing_device_reports(self);
                _collect_gamepad_reports(self);
                break;
        }


        mtx_unlock(&self->mtx);
        vcpu_sigwait(&self->reportsWaitQueue, &self->reportSigs, &signo);
        mtx_lock(&self->mtx);
    }

    mtx_unlock(&self->mtx);
}
