//
//  HIDManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "HIDManagerPriv.h"
#include <driver/DriverManager.h>
#include <filesystem/IOChannel.h>
#include <kern/kalloc.h>
#include <kern/limits.h>
#include <kpi/fcntl.h>
#include <process/Process.h>


extern const uint8_t gUSBHIDKeyFlags[256];
int _vbl_handler(HIDManagerRef _Nonnull self);
static void _collect_framebuffer_size(HIDManagerRef _Nonnull self);
static void _reports_collector_loop(HIDManagerRef _Nonnull self);


HIDManagerRef _Nonnull gHIDManager;


errno_t HIDManager_Create(HIDManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    HIDManagerRef self = NULL;
    
    try(kalloc_cleared(sizeof(HIDManager), (void**)&self));

    mtx_init(&self->mtx);
    wq_init(&self->reportsWaitQueue);

    self->reportSigs = _SIGBIT(SIGKEY) | _SIGBIT(SIGVBL) | _SIGBIT(SIGSCR);
    self->report.type = kHIDReportType_Null;

    self->keyFlags = gUSBHIDKeyFlags;
    self->isMouseMoveReportingEnabled = false;
    self->hiddenCount = 0;


    // Create the HID event queue
    const size_t powOf2Capacity = siz_pow2_ceil(REPORT_QUEUE_MAX_EVENTS);
    
    try(kalloc_cleared(powOf2Capacity * sizeof(HIDEvent), (void**)&self->evqQueue));
    self->evqCapacity = powOf2Capacity;
    self->evqCapacityMask = powOf2Capacity - 1;
    self->evqReadIdx = 0;
    self->evqWriteIdx = 0;
    self->evqOverflowCount = 0;
    HIDEventSynth_Init(&self->evqSynth);
    cnd_init(&self->evqCnd);


    self->vblHandler.id = IRQ_ID_VBLANK;
    self->vblHandler.priority = IRQ_PRI_HIGHEST + 8;
    self->vblHandler.enabled = true;
    self->vblHandler.func = (irq_handler_func_t)_vbl_handler;
    self->vblHandler.arg = self;


catch:
    *pOutSelf = self;
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
    if (pInitialDelay) {
        *pInitialDelay = self->evqSynth.initialKeyRepeatDelay;
    }
    if (pRepeatDelay) {
        *pRepeatDelay = self->evqSynth.keyRepeatDelay;
    }
    mtx_unlock(&self->mtx);
}

void HIDManager_SetKeyRepeatDelays(HIDManagerRef _Nonnull self, const struct timespec* _Nonnull initialDelay, const struct timespec* _Nonnull repeatDelay)
{
    mtx_lock(&self->mtx);
    self->evqSynth.initialKeyRepeatDelay = *initialDelay;
    self->evqSynth.keyRepeatDelay = *repeatDelay;
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

errno_t HIDManager_ObtainCursor(HIDManagerRef _Nonnull self, int width, int height, PixelFormat pixelFormat)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    if (self->fb) {
        err = GraphicsDriver_ObtainMouseCursor(self->fb, width, height, pixelFormat);
        if (err == EOK) {
            self->cursorWidth = width;
            self->cursorHeight = height;
            self->hiddenCount = 0;
            self->isMouseObscured = false;
            self->isMouseShielded = false;
            self->isMouseShieldEnabled = false;
            self->mouse.x = 0;
            self->mouse.y = 0;
            GraphicsDriver_SetMouseCursorVisible(self->fb, true);
        }
    }
    else {
        err = ENODEV;
    }
    mtx_unlock(&self->mtx);

    return err;
}

void HIDManager_ReleaseCursor(HIDManagerRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    if (self->fb) {
        GraphicsDriver_ReleaseMouseCursor(self->fb);
        self->cursorWidth = 0;
        self->cursorHeight = 0;
    }
    mtx_unlock(&self->mtx);
}

errno_t HIDManager_SetCursor(HIDManagerRef _Nonnull self, const uint16_t* _Nullable planes[2], int hotSpotX, int hotSpotY)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    if (hotSpotX < 0 || hotSpotX > self->cursorWidth || hotSpotY < 0 || hotSpotY > self->cursorHeight) {
        throw(EINVAL);
    }

    if (self->fb) {
        try(GraphicsDriver_SetMouseCursor(self->fb, planes));
        self->hotSpotX = hotSpotX;
        self->hotSpotY = hotSpotY;
    }
    else {
        err = ENODEV;
    }

catch:
    mtx_unlock(&self->mtx);
    return err;
}


static bool _show_cursor(HIDManagerRef _Nonnull _Locked self)
{
    if (self->hiddenCount > 0) {
        self->hiddenCount--;
    }

    if (self->hiddenCount == 0) {
        GraphicsDriver_SetMouseCursorPosition(self->fb, self->mouse.x, self->mouse.y);
        GraphicsDriver_SetMouseCursorVisible(self->fb, true);
        return true;
    }
    else {
        return false;
    }
}

void HIDManager_ShowCursor(HIDManagerRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    if (_show_cursor(self)) {
        self->isMouseShieldEnabled = false;
        self->isMouseObscured = false;
    }
    mtx_unlock(&self->mtx);
}

static void _hide_cursor(HIDManagerRef _Nonnull _Locked self)
{
    if (self->hiddenCount == 0) {
        GraphicsDriver_SetMouseCursorVisible(self->fb, false);
    }
    if (self->hiddenCount < INT_MAX) {
        self->hiddenCount++;
    }
}

void HIDManager_HideCursor(HIDManagerRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    _hide_cursor(self);
    mtx_unlock(&self->mtx);
}

void HIDManager_ObscureCursor(HIDManagerRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    if (self->hiddenCount == 0) {
        self->isMouseObscured = true;
        GraphicsDriver_SetMouseCursorVisible(self->fb, false);
    }
    mtx_unlock(&self->mtx);
}

static bool _shield_intersects_cursor(HIDManagerRef _Nonnull self)
{
    int r;

    self->cursorBounds.l = self->mouse.x - self->hotSpotX;
    self->cursorBounds.t = self->mouse.y - self->hotSpotY;
    self->cursorBounds.r = self->cursorBounds.l + self->cursorWidth;
    self->cursorBounds.b = self->cursorBounds.b + self->cursorHeight;
    hid_rect_intersects(&self->shieldRect, &self->cursorBounds, r);

    return (r) ? true : false;
}

#define _shield_cursor(__self) \
_hide_cursor(__self); \
(__self)->isMouseShielded = true

#define _unshield_cursor(__self) \
_show_cursor(__self); \
(__self)->isMouseShielded = false

errno_t HIDManager_ShieldMouseCursor(HIDManagerRef _Nonnull self, int x, int y, int width, int height)
{
    if (width < 0 || height < 0) {
        return EINVAL;
    }

    mtx_lock(&self->mtx);
    // No need to shield if we're hidden already
    if (self->hiddenCount == 0) {
        int l = x;
        int t = y;
        int r = x + width;
        int b = y + height;

        self->shieldRect.l = __max(__min(l, INT16_MAX), 0);
        self->shieldRect.t = __max(__min(t, INT16_MAX), 0);
        self->shieldRect.r = __max(__min(r, INT16_MAX), 0);
        self->shieldRect.b = __max(__min(b, INT16_MAX), 0);

        self->isMouseShieldEnabled = ((self->shieldRect.r - self->shieldRect.l) > 0 && (self->shieldRect.b - self->shieldRect.t) > 0) ? true : false;

        if (self->isMouseShieldEnabled) {
            if (_shield_intersects_cursor(self)) {
                _shield_cursor(self);
            }
        }
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
// MARK: Event Queue
////////////////////////////////////////////////////////////////////////////////

// Removes all events from the queue.
void HIDManager_FlushEvents(HIDManagerRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    self->evqReadIdx = 0;
    self->evqWriteIdx = 0;
    HIDEventSynth_Reset(&self->evqSynth);
    mtx_unlock(&self->mtx);
}

// Posts the given event to the queue. This event replaces the oldest event
// in the queue if the queue is full. This function must be called from the
// interrupt context.
static void _post_event(HIDManagerRef _Nonnull _Locked self, HIDEventType type, did_t driverId, const HIDEventData* _Nonnull pEventData)
{
    if (EVQ_WRITABLE_COUNT() > 0) {
        HIDEvent* pe = &self->evqQueue[self->evqWriteIdx++ & self->evqCapacityMask];

        pe->type = type;
        pe->driverId = driverId;
        pe->eventTime = self->now;
        pe->data = *pEventData;
    
        cnd_broadcast(&self->evqCnd);
    }
    else {
        self->evqOverflowCount++;
    }
}

void HIDManager_PostEvent(HIDManagerRef _Nonnull self, HIDEventType type, did_t driverId, const HIDEventData* _Nonnull pEventData)
{
    mtx_lock(&self->mtx);
    clock_gettime(g_mono_clock, &self->now);
    _post_event(self, type, driverId, pEventData);
    mtx_unlock(&self->mtx);
}

// Dequeues and returns the next available event or ETIMEDOUT if no event is
// available and a timeout > 0 was specified. Returns EAGAIN if no event is
// available and the timeout is 0.
errno_t HIDManager_GetNextEvent(HIDManagerRef _Nonnull self, const struct timespec* _Nonnull timeout, HIDEvent* _Nonnull evt)
{
    decl_try_err();
    struct timespec deadline;

    mtx_lock(&self->mtx);
    for (;;) {
        const HIDEvent* queue_evt = NULL;

        if (EVQ_READABLE_COUNT() > 0) {
            queue_evt = &self->evqQueue[self->evqReadIdx & self->evqCapacityMask];
        }


        const HIDSynthAction t = HIDEventSynth_Tick(&self->evqSynth, queue_evt, &self->evqSynthResult);
        if (t == HIDSynth_UseEvent) {
            *evt = *queue_evt;
            self->evqReadIdx++;
            break;
        }
        if (t == HIDSynth_MakeRepeat) {
            evt->type = kHIDEventType_KeyDown;
            evt->eventTime = self->evqSynthResult.deadline;
            evt->data.key.flags = self->evqSynthResult.flags;
            evt->data.key.keyCode = self->evqSynthResult.keyCode;
            evt->data.key.isRepeat = true;
            break;
        }

        if (t == HIDSynth_Wait) {
            deadline = *timeout;
        }
        else {
            deadline = (timespec_lt(&self->evqSynthResult.deadline, timeout)) ? self->evqSynthResult.deadline : *timeout;
        }
        if (deadline.tv_sec == 0 && deadline.tv_nsec == 0) {
            err = EAGAIN;
            break;
        }


        err = cnd_timedwait(&self->evqCnd, &self->mtx, &deadline);
        if (err == ETIMEDOUT) {
            struct timespec now;

            clock_gettime(g_mono_clock, &now);
            if (timespec_ge(&now, timeout)) {
                break;
            }
            err = EOK;
        }
        else if (err != EOK) {
            break;
        }
    }
    mtx_unlock(&self->mtx);

    return err;
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

    _post_event(self, evtType, 0, &evt);
}

// Posts suitable mouse events to the event queue.
static void _post_mouse_event(HIDManagerRef _Nonnull _Locked self, bool hasPositionChange, bool hasButtonsChange, uint32_t oldButtonsDown)
{
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
                _post_event(self, evtType, 0, &evt);
            }
        }
    }
    
    if (hasPositionChange && self->isMouseMoveReportingEnabled) {
        HIDEventData evt;

        evt.mouseMoved.flags = self->modifierFlags;
        evt.mouseMoved.x = self->mouse.x;
        evt.mouseMoved.y = self->mouse.y;
        _post_event(self, kHIDEventType_MouseMoved, 0, &evt);
    }
}

// Reports a change in the state of a gamepad style device. Posts suitable events
// to the event queue.
static void _post_gamepad_event(HIDManagerRef _Nonnull _Locked self, gamepad_state_t* _Nonnull gp, const HIDReport* _Nonnull report)
{
    did_t did = Driver_GetId(IOChannel_GetResourceAs(gp->ch, Driver));

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

                evt.joystick.buttonNumber = i;
                evt.joystick.flags = self->modifierFlags;
                evt.joystick.dx = report->data.joy.x;
                evt.joystick.dy = report->data.joy.y;
                _post_event(self, evtType, did, &evt);
            }
        }
    }
    
    
    // Generate motion events
    const int16_t diffX = report->data.joy.x - gp->x;
    const int16_t diffY = report->data.joy.y - gp->y;
    
    if (diffX != 0 || diffY != 0) {
        HIDEventData evt;

        evt.joystickMotion.dx = report->data.joy.x;
        evt.joystickMotion.dy = report->data.joy.y;
        _post_event(self, kHIDEventType_JoystickMotion, did, &evt);
    }

    gp->x = report->data.joy.x;
    gp->y = report->data.joy.y;
    gp->buttons = report->data.joy.buttons;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: HID Reports Collector
////////////////////////////////////////////////////////////////////////////////

IOCATS_DEF(g_hid_cats, IOHID_KEYBOARD, IOHID_KEYPAD, IOHID_MOUSE, IOHID_LIGHTPEN,
    IOHID_STYLUS, IOHID_TRACKBALL, IOHID_ANALOG_JOYSTICK, IOHID_DIGITAL_JOYSTICK,
    IOVID_FB);

IOCATS_DEF(g_gamepad_cats, IOHID_ANALOG_JOYSTICK, IOHID_DIGITAL_JOYSTICK);

IOCATS_DEF(g_pointing_device_cats, IOHID_MOUSE, IOHID_TRACKBALL, IOHID_LIGHTPEN,
    IOHID_STYLUS);


// Connects the given HID input driver or framebuffer to the HID manager by
// opening a channel to it.
static void _connect_driver(HIDManagerRef _Nonnull _Locked self, DriverRef _Nonnull driver)
{
    decl_try_err();

    if (self->kbChannel == NULL && Driver_HasCategory(driver, IOHID_KEYBOARD)) {
        err = Driver_Open(driver, O_RDWR, 0, &self->kbChannel);
        if (err == EOK) {
            InputDriver_SetReportTarget(driver, self->reportsCollector, SIGKEY);
            self->kb = (InputDriverRef)driver;
        }
    }
    else if (self->mouse.chCount < MAX_POINTING_DEVICES && Driver_HasSomeCategories(driver, g_pointing_device_cats)) {
        for (int i = 0; i < MAX_POINTING_DEVICES; i++) {
            if (self->mouse.ch[i] == NULL) {
                err = Driver_Open(driver, O_RDWR, 0, &self->mouse.ch[i]);
                if (err == EOK) {
                    if (Driver_HasCategory(driver, IOHID_LIGHTPEN) && self->fb) {
                        self->mouse.lpCount++;
                        if (self->mouse.lpCount == 1) {
                            GraphicsDriver_SetLightPenEnabled(self->fb, true);
                        }
                    }
                    self->mouse.chCount++;
                }
                break;
            }
        }
    }
    else if (self->gamepadCount < MAX_GAME_PADS && Driver_HasSomeCategories(driver, g_gamepad_cats)) {
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
    else if (self->fbChannel == NULL && Driver_HasCategory(driver, IOVID_FB)) {
        // Open a channel to the framebuffer
        err = Driver_Open(driver, O_RDWR, 0, &self->fbChannel);
        if (err == EOK) {
            self->fb = IOChannel_GetResourceAs(self->fbChannel, GraphicsDriver);
            GraphicsDriver_SetScreenConfigObserver(self->fb, self->reportsCollector, SIGSCR);
            _collect_framebuffer_size(self);
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
        GraphicsDriver_SetScreenConfigObserver(self->fb, NULL, 0);
        IOChannel_Release(self->fbChannel);
        self->fbChannel = NULL;
        self->fb = NULL;
        hid_rect_set_empty(&self->screenBounds);
        return;
    }

    for (int i = 0; i < MAX_POINTING_DEVICES; i++) {
        IOChannelRef ch = self->mouse.ch[i];
        DriverRef cdp = (ch) ? IOChannel_GetResourceAs(ch, Driver) : NULL;

        if (cdp == driver) {
            if (self->mouse.lpCount > 0 && Driver_HasCategory(cdp, IOHID_LIGHTPEN)) {
                self->mouse.lpCount--;
                if (self->mouse.lpCount == 0) {
                    GraphicsDriver_SetLightPenEnabled(self->fb, false);
                }
            }
            IOChannel_Release(ch);
            self->mouse.ch[i] = NULL;
            self->mouse.chCount--;
            break;
        }
    }

    for (int i = 0; i < MAX_GAME_PADS; i++) {
        gamepad_state_t* gp = &self->gamepad[i];

        if (gp->ch && IOChannel_GetResourceAs(gp->ch, Driver) == driver) {
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
        case IONOTIFY_STARTED:
            _connect_driver(self, driver);
            break;

        case IONOTIFY_STOPPING:
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
    uint32_t newButtons = 0;

    // Collect reports from all devices that control the logical mouse and compute
    // the new logical mouse state
    for (int i = 0; i < self->mouse.chCount; i++) {
        IOChannelRef ch = self->mouse.ch[i];

        if (ch) {
            int16_t dx, dy;
            uint32_t bt;

            InputDriver_GetReport(IOChannel_GetResourceAs(ch, InputDriver), &self->report);

            switch (self->report.type) {
                case kHIDReportType_Mouse:
                    dx = self->report.data.mouse.dx;
                    dy = self->report.data.mouse.dy;
                    bt = self->report.data.mouse.buttons;
                    break;

                case kHIDReportType_LightPen:
                    dx = (self->report.data.lp.hasPosition) ? self->report.data.lp.x - self->mouse.x : 0;
                    dy = (self->report.data.lp.hasPosition) ? self->report.data.lp.y - self->mouse.y : 0;
                    bt = self->report.data.lp.buttons;
                    break;

                default:
                    dx = 0;
                    dy = 0;
                    bt = 0;
                    break;
            }

            if (dx != 0 || dy != 0) {
                const int16_t mx = self->mouse.x + dx;
                const int16_t my = self->mouse.y + dy;

                self->mouse.x = __min(__max(mx, self->screenBounds.l), self->screenBounds.r - 1);
                self->mouse.y = __min(__max(my, self->screenBounds.t), self->screenBounds.b - 1);
            }
            newButtons |= bt;
        }
    }
    self->mouse.buttons = newButtons;


    const bool hasButtonsChange = (oldButtonsDown != self->mouse.buttons);
    const bool hasPositionChange = (oldX != self->mouse.x || oldY != self->mouse.y);


    // Move the mouse cursor image on screen if the mouse position has changed
    if (hasPositionChange) {
        if (self->isMouseShieldEnabled && self->fb) {
            if (_shield_intersects_cursor(self)) {
                if (!self->isMouseShielded) {
                    _shield_cursor(self);
                }
            }
            else {
                if (self->isMouseShielded) {
                    _unshield_cursor(self);
                }
            }
        }

        if (self->hiddenCount == 0 && self->fb) {
            GraphicsDriver_SetMouseCursorPosition(self->fb, self->mouse.x - self->hotSpotX, self->mouse.y - self->hotSpotY);
            if (self->isMouseObscured) {
                GraphicsDriver_SetMouseCursorVisible(self->fb, true);
                self->isMouseObscured = false;
            }
        }
    }


    // Post mouse events
    _post_mouse_event(self, hasPositionChange, hasButtonsChange, oldButtonsDown);
}

static void _collect_gamepad_reports(HIDManagerRef _Nonnull self)
{
    for (int i = 0; i < self->gamepadCount; i++) {
        gamepad_state_t* gp = &self->gamepad[i];

        if (gp->ch) {
            InputDriver_GetReport(IOChannel_GetResourceAs(gp->ch, InputDriver), &self->report);
            _post_gamepad_event(self, gp, &self->report);
        }
    }
}

static void _collect_framebuffer_size(HIDManagerRef _Nonnull self)
{
    bool hasChanged = false;
    int w, h;
    GraphicsDriver_GetScreenSize(self->fb, &w, &h);

    self->screenBounds.l = 0;
    self->screenBounds.t = 0;
    self->screenBounds.r = (int16_t)w;
    self->screenBounds.b = (int16_t)h;

    if (self->mouse.x >= w) {
        self->mouse.x = w - 1;
        hasChanged = true;
    }
    if (self->mouse.y >= h) {
        self->mouse.y = h - 1;
        hasChanged = true;
    }

    if (hasChanged) {
        GraphicsDriver_SetMouseCursorPosition(self->fb, self->mouse.x - self->hotSpotX, self->mouse.y - self->hotSpotY);
    }
}


static void _reports_collector_loop(HIDManagerRef _Nonnull self)
{
    int signo = 0;

    DriverManager_StartMatching(gDriverManager, g_hid_cats, (drv_match_func_t)_matching_driver, self);

    mtx_lock(&self->mtx);

    for (;;) {
        self->report.type = kHIDReportType_Null;
        clock_gettime(g_mono_clock, &self->now);


        switch (signo) {
            case SIGKEY:
                _collect_keyboard_reports(self);
                break;

            case SIGVBL:
                _collect_pointing_device_reports(self);
                _collect_gamepad_reports(self);
                break;

            case SIGSCR:
                _collect_framebuffer_size(self);
                break;
        }


        mtx_unlock(&self->mtx);
        vcpu_sigwait(&self->reportsWaitQueue, &self->reportSigs, &signo);
        mtx_lock(&self->mtx);
    }

    mtx_unlock(&self->mtx);
}
