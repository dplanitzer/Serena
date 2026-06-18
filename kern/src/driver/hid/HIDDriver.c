//
//  HIDDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "HIDDriverPriv.h"
#include <assert.h>
#include <limits.h>
#include <driver/hw/m68k-amiga/graphics/GraphicsDriver.h>
#include <driver/hw/m68k-amiga/hid/KeyboardDriver.h>
#include <driver/hw/m68k-amiga/hid/MouseDriver.h>
#include <driver/IOCatalog.h>
#include <ext/bit.h>
#include <ext/math.h>
#include <ext/nanotime.h>
#include <kern/kalloc.h>
#include <kpi/fd.h>
#include <kpi/file.h>
#include <kpi/hid.h>
#include <process/kerneld.h>


IOCATS_DEF(g_cats, IOHID_MANAGER);

extern const uint8_t gUSBHIDKeyFlags[256];
void _vbl_handler(HIDDriverRef _Nonnull self);
static void _collect_framebuffer_size(HIDDriverRef _Nonnull self);
static void _reports_collector_loop(HIDDriverRef _Nonnull self);


errno_t HIDDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    HIDDriverRef self = NULL;

    try(Driver_CreateRoot(class(HIDDriver), 0, g_cats, (DriverRef*)&self));

    mtx_init(&self->mtx);
    wq_init(&self->reportsWaitQueue);

    self->reportSigs = sig_bit(SIGVBL) | sig_bit(SIGSCR);
    self->report.type = kHIDReportType_Null;

    self->keyFlags = gUSBHIDKeyFlags;
    self->isMouseMoveReportingEnabled = false;
    self->hiddenCount = 0;
    // We set the initial mouse position to 2,2 to ensure that the whole mouse
    // cursor will show up in the top left screen corner (assuming that it's
    // hot spot offset will be 1,1 in hires pixels).
    self->mouse.x = 2;
    self->mouse.y = 2;


    // Create the HID event queue
    const size_t powOf2Capacity = pow2_ceil_sz(REPORT_QUEUE_MAX_EVENTS);
    
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
    *pOutSelf = (DriverRef)self;
    return err;
}

errno_t HIDDriver_onStart(HIDDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    DriverEntry de;

    // Create the event vcpu
    vcpu_attr_t attr;
    attr.version = sizeof(vcpu_attr_t);
    attr.stack_size = 0;
    attr.group_id = VCPUID_MAIN_GROUP;
    attr.policy.version = sizeof(vcpu_policy_t);
    attr.policy.qos.grade = VCPU_QOS_REALTIME;
    attr.policy.qos.priority = VCPU_PRI_HIGHEST - 1;
    attr.flags = _VCPU_RESUMED;
    try(Process_AcquireVirtualProcessor(gKernelProcess, (vcpu_func_t)_reports_collector_loop, self, &attr, 0, &self->reportsCollector));


    // Enable VBL interrupts
    irq_add_handler(&self->vblHandler);

    de.name = "hid";
    de.uid = UID_ROOT;
    de.gid = GID_ROOT;
    de.perms = fs_perms_from_octal(0666);
    de.arg = 0;

    try(Driver_Publish((DriverRef)self, &de));

catch:
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Kernel API
////////////////////////////////////////////////////////////////////////////////

void HIDDriver_GetKeyRepeatDelays(HIDDriverRef _Nonnull self, nanotime_t* _Nullable pInitialDelay, nanotime_t* _Nullable pRepeatDelay)
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

void HIDDriver_SetKeyRepeatDelays(HIDDriverRef _Nonnull self, const nanotime_t* _Nonnull initialDelay, const nanotime_t* _Nonnull repeatDelay)
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
void HIDDriver_GetDeviceKeysDown(HIDDriverRef _Nonnull self, const HIDKeyCode* _Nullable pKeysToCheck, int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, int* _Nonnull nKeysDown)
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

errno_t HIDDriver_ObtainCursor(HIDDriverRef _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    if (self->fb) {
        err = DisplayDriver_ObtainCursor(self->fb);
        if (err == EOK) {
            self->cursorWidth = 0;
            self->cursorHeight = 0;
            self->hotSpotX = 0;
            self->hotSpotY = 0;
            self->hiddenCount = 0;
            self->isMouseObscured = false;
            self->isMouseShielded = false;
            self->isMouseShieldEnabled = false;
        }
    }
    else {
        err = ENODEV;
    }
    mtx_unlock(&self->mtx);

    return err;
}

void HIDDriver_ReleaseCursor(HIDDriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    if (self->fb) {
        DisplayDriver_ReleaseCursor(self->fb);
        GraphicsDriver_DestroySurface(self->fb, self->cursorSurfaceId);
        self->cursorSurfaceId = 0;
    }
    mtx_unlock(&self->mtx);
}

errno_t HIDDriver_SetCursor(HIDDriverRef _Nonnull self, const void* _Nullable planes[], size_t bytesPerRow, int width, int height, pixfmt_t format, int hotSpotX, int hotSpotY)
{
    decl_try_err();

    if ((planes && bytesPerRow == 0) || width != kCursor_Width || height != kCursor_Height || format != kCursor_PixelFormat) {
        return EINVAL;
    }
    if (hotSpotX < 0 || hotSpotX >= width || hotSpotY < 0 || hotSpotY >= height) {
        return EINVAL;
    }


    mtx_lock(&self->mtx);
    if (self->fb == NULL) {
        throw(ENODEV);
    }


    if (self->cursorSurfaceId == 0 || self->cursorWidth != width || self->cursorHeight != height) {
        int newId;

        try(GraphicsDriver_CreateSurface2d(self->fb, width, height, PIXFMT_RGB_SPRITE_2, &newId));
        self->cursorWidth = width;
        self->cursorHeight = height;

        if (self->cursorSurfaceId) {
            GraphicsDriver_DestroySurface(self->fb, self->cursorSurfaceId);
        }
        self->cursorSurfaceId = newId;
    }

    try(GraphicsDriver_WritePixels(self->fb, self->cursorSurfaceId, planes, bytesPerRow, format));
    try(DisplayDriver_BindCursor(self->fb, self->cursorSurfaceId));
    self->hotSpotX = hotSpotX;
    self->hotSpotY = hotSpotY;

    // Update the framebuffer mouse cursor position to line it up with our position
    // and the new hot spot offset.
    DisplayDriver_SetCursorPosition(self->fb, self->mouse.x - hotSpotX, self->mouse.y - hotSpotY);

catch:
    mtx_unlock(&self->mtx);
    return err;
}


static bool _show_cursor(HIDDriverRef _Nonnull _Locked self)
{
    if (self->hiddenCount > 0) {
        self->hiddenCount--;
    }

    if (self->hiddenCount == 0) {
        DisplayDriver_SetCursorPosition(self->fb, self->mouse.x - self->hotSpotX, self->mouse.y - self->hotSpotY);
        DisplayDriver_SetCursorVisible(self->fb, true);
        return true;
    }
    else {
        return false;
    }
}

void HIDDriver_ShowCursor(HIDDriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    if (_show_cursor(self)) {
        self->isMouseShieldEnabled = false;
        self->isMouseObscured = false;
    }
    mtx_unlock(&self->mtx);
}

static void _hide_cursor(HIDDriverRef _Nonnull _Locked self)
{
    if (self->hiddenCount == 0) {
        DisplayDriver_SetCursorVisible(self->fb, false);
    }
    if (self->hiddenCount < INT_MAX) {
        self->hiddenCount++;
    }
}

void HIDDriver_HideCursor(HIDDriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    _hide_cursor(self);
    mtx_unlock(&self->mtx);
}

void HIDDriver_ObscureCursor(HIDDriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    if (self->hiddenCount == 0) {
        self->isMouseObscured = true;
        DisplayDriver_SetCursorVisible(self->fb, false);
    }
    mtx_unlock(&self->mtx);
}

static bool _shield_intersects_cursor(HIDDriverRef _Nonnull self)
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

errno_t HIDDriver_ShieldMouseCursor(HIDDriverRef _Nonnull self, int x, int y, int width, int height)
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
void HIDDriver_GetMouseDevicePosition(HIDDriverRef _Nonnull self, int* _Nonnull pOutX, int* _Nonnull pOutY)
{
    mtx_lock(&self->mtx);
    *pOutX = self->mouse.x;
    *pOutY = self->mouse.y;
    mtx_unlock(&self->mtx);
}

// Returns a bit mask of all the mouse buttons that are currently pressed.
uint32_t HIDDriver_GetMouseDeviceButtonsDown(HIDDriverRef _Nonnull self)
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
void HIDDriver_FlushEvents(HIDDriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    self->evqReadIdx = 0;
    self->evqWriteIdx = 0;
    HIDEventSynth_Reset(&self->evqSynth);
    mtx_unlock(&self->mtx);
}

// Queues the given event. This event replaces the oldest event in the queue if
// the queue is full. This function must be called from the interrupt context.
// NOTE: this function does not do a cnd_broadcast() the caller must do this
// before dropping the lock.
static void _queue_event(HIDDriverRef _Nonnull _Locked self, HIDEventType type, did_t driverId, const HIDEventData* _Nonnull pEventData)
{
    if (EVQ_WRITABLE_COUNT() > 0) {
        HIDEvent* pe = &self->evqQueue[self->evqWriteIdx++ & self->evqCapacityMask];

        pe->type = type;
        pe->driverId = driverId;
        pe->eventTime = self->now;
        pe->data = *pEventData;
    }
    else {
        self->evqOverflowCount++;
    }
}

void HIDDriver_PostEvent(HIDDriverRef _Nonnull self, HIDEventType type, did_t driverId, const HIDEventData* _Nonnull pEventData)
{
    mtx_lock(&self->mtx);
    clock_gettime(g_mono_clock, &self->now);
    _queue_event(self, type, driverId, pEventData);
    cnd_broadcast_with_boost(&self->evqCnd, VCPU_PRI_HIGHEST);
    mtx_unlock(&self->mtx);
}

// Dequeues and returns the next available event or ETIMEDOUT if no event is
// available and a timeout > 0 was specified. Returns EAGAIN if no event is
// available and the timeout is 0.
errno_t HIDDriver_GetNextEvent(HIDDriverRef _Nonnull self, const nanotime_t* _Nonnull timeout, HIDEvent* _Nonnull evt)
{
    decl_try_err();
    nanotime_t deadline;

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
            deadline = (nanotime_lt(&self->evqSynthResult.deadline, timeout)) ? self->evqSynthResult.deadline : *timeout;
        }
        if (deadline.tv_sec == 0 && deadline.tv_nsec == 0) {
            err = EAGAIN;
            break;
        }


        err = cnd_timedwait(&self->evqCnd, &self->mtx, &deadline);
        if (err == ETIMEDOUT) {
            nanotime_t now;

            clock_gettime(g_mono_clock, &now);
            if (nanotime_ge(&now, timeout)) {
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
static void _post_key_event(HIDDriverRef _Nonnull _Locked self, const HIDReport* _Nonnull report)
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

    _queue_event(self, evtType, 0, &evt);
}

// Posts suitable mouse events to the event queue.
static void _post_mouse_event(HIDDriverRef _Nonnull _Locked self, bool hasPositionChange, bool hasButtonsChange, uint32_t oldButtonsDown)
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
                _queue_event(self, evtType, 0, &evt);
            }
        }
    }
    
    if (hasPositionChange && self->isMouseMoveReportingEnabled) {
        HIDEventData evt;

        evt.mouseMoved.flags = self->modifierFlags;
        evt.mouseMoved.x = self->mouse.x;
        evt.mouseMoved.y = self->mouse.y;
        _queue_event(self, kHIDEventType_MouseMoved, 0, &evt);
    }
}

// Reports a change in the state of a gamepad style device. Posts suitable events
// to the event queue.
static void _post_gamepad_event(HIDDriverRef _Nonnull _Locked self, gamepad_state_t* _Nonnull gp, const HIDReport* _Nonnull report)
{
    did_t did = Driver_GetId(gp->drv);

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
                _queue_event(self, evtType, did, &evt);
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
        _queue_event(self, kHIDEventType_JoystickMotion, did, &evt);
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
static void _connect_driver(HIDDriverRef _Nonnull _Locked self, DriverRef _Nonnull driver)
{
    decl_try_err();

    if (self->kb == NULL && Driver_HasCategory(driver, IOHID_KEYBOARD)) {
        err = Driver_Open(driver, O_RDWR, 0, NULL);
        if (err == EOK) {
            self->kb = Object_RetainAs(driver, KeyboardDriver);
        }
    }
    else if (self->mouse.drvCount < MAX_POINTING_DEVICES && Driver_HasSomeCategories(driver, g_pointing_device_cats)) {
        for (int i = 0; i < MAX_POINTING_DEVICES; i++) {
            if (self->mouse.drv[i] == NULL) {
                err = Driver_Open(driver, O_RDWR, 0, NULL);
                if (err == EOK) {
                    if (Driver_HasCategory(driver, IOHID_LIGHTPEN) && self->fb) {
                        self->mouse.lpCount++;
                        if (self->mouse.lpCount == 1) {
                            DisplayDriver_SetLightPenEnabled(self->fb, true);
                        }
                    }

                    self->mouse.drv[i] = Object_RetainAs(driver, Driver);
                    self->mouse.drvCount++;
                }
                break;
            }
        }
    }
    else if (self->gamepadCount < MAX_GAME_PADS && Driver_HasSomeCategories(driver, g_gamepad_cats)) {
        for (int i = 0; i < MAX_GAME_PADS; i++) {
            gamepad_state_t* gp = &self->gamepad[i];

            if (gp->drv == NULL) {
                err = Driver_Open(driver, O_RDWR, 0, NULL);
                if (err == EOK) {
                    gp->drv = Object_RetainAs(driver, Driver);
                    gp->x = 0;
                    gp->y = 0;
                    gp->buttons = 0;
                    self->gamepadCount++;
                }
                break;
            }
        }
    }
    else if (self->fb == NULL && Driver_HasCategory(driver, IOVID_FB)) {
        // Open a channel to the framebuffer
        err = Driver_Open(driver, O_RDWR, 0, NULL);
        if (err == EOK) {
            self->fb = Object_RetainAs(driver, GraphicsDriver);
            DisplayDriver_SetScreenConfigObserver(self->fb, self->reportsCollector, SIGSCR);
            _collect_framebuffer_size(self);
        }
    }
}

// Disconnects the given HID driver or framebuffer.
static void _disconnect_driver(HIDDriverRef _Nonnull _Locked self, DriverRef _Nonnull driver)
{
    if ((DriverRef)self->kb == driver) {
        Driver_Close(self->kb);
        Object_Release(self->kb);
        self->kb = NULL;
        return;
    }
    
    if ((DriverRef)self->fb == driver) {
        DisplayDriver_SetScreenConfigObserver(self->fb, NULL, 0);

        Driver_Close(self->fb);
        Object_Release(self->fb);
        self->fb = NULL;
        hid_rect_set_empty(&self->screenBounds);
        return;
    }

    for (int i = 0; i < MAX_POINTING_DEVICES; i++) {
        DriverRef cdp = self->mouse.drv[i];

        if (cdp == driver) {
            if (self->mouse.lpCount > 0 && Driver_HasCategory(cdp, IOHID_LIGHTPEN)) {
                self->mouse.lpCount--;
                if (self->mouse.lpCount == 0) {
                    DisplayDriver_SetLightPenEnabled(self->fb, false);
                }
            }

            Driver_Close(cdp);
            Object_Release(cdp);
            self->mouse.drv[i] = NULL;
            self->mouse.drvCount--;
            break;
        }
    }

    for (int i = 0; i < MAX_GAME_PADS; i++) {
        gamepad_state_t* gp = &self->gamepad[i];

        if (gp->drv == driver) {
            Driver_Close(gp->drv);
            Object_Release(gp->drv);
            gp->drv = NULL;
            self->gamepadCount--;
            break;
        }
    }
}

static void _matching_driver(HIDDriverRef _Nonnull self, DriverRef _Nonnull driver, int action)
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


void _vbl_handler(HIDDriverRef _Nonnull self)
{
    vcpu_send_signal(self->reportsCollector, SIGVBL);
}


static bool _collect_keyboard_reports(HIDDriverRef _Nonnull self)
{
    bool r = false;

    for (;;) {
        InputDriver_GetReport(self->kb, &self->report);
                
        if (self->report.type == kHIDReportType_Null) {
            break;
        }

        _post_key_event(self, &self->report);
        r = true;
    }

    return r;
}

static bool _collect_pointing_device_reports(HIDDriverRef _Nonnull self)
{
    if (self->mouse.drvCount == 0) {
        return false;
    }

    const int16_t oldX = self->mouse.x;
    const int16_t oldY = self->mouse.y;
    const uint32_t oldButtonsDown = self->mouse.buttons;
    uint32_t newButtons = 0;

    // Collect reports from all devices that control the logical mouse and compute
    // the new logical mouse state
    for (int i = 0; i < self->mouse.drvCount; i++) {
        DriverRef d = self->mouse.drv[i];

        if (d) {
            int16_t dx, dy;
            uint32_t bt;

            InputDriver_GetReport(d, &self->report);

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
            DisplayDriver_SetCursorPosition(self->fb, self->mouse.x - self->hotSpotX, self->mouse.y - self->hotSpotY);
            if (self->isMouseObscured) {
                DisplayDriver_SetCursorVisible(self->fb, true);
                self->isMouseObscured = false;
            }
        }
    }


    // Post mouse events
    _post_mouse_event(self, hasPositionChange, hasButtonsChange, oldButtonsDown);

    return true;
}

static bool _collect_gamepad_reports(HIDDriverRef _Nonnull self)
{
    bool r = false;

    for (int i = 0; i < self->gamepadCount; i++) {
        gamepad_state_t* gp = &self->gamepad[i];

        if (gp->drv) {
            InputDriver_GetReport(gp->drv, &self->report);
            _post_gamepad_event(self, gp, &self->report);
            r = true;
        }
    }

    return r;
}

static void _collect_framebuffer_size(HIDDriverRef _Nonnull self)
{
    bool hasChanged = false;
    int w, h;
    DisplayDriver_GetScreenSize(self->fb, &w, &h);

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
        DisplayDriver_SetCursorPosition(self->fb, self->mouse.x - self->hotSpotX, self->mouse.y - self->hotSpotY);
    }
}


static void _reports_collector_loop(HIDDriverRef _Nonnull self)
{
    int signo = 0;

    IOCatalog_StartMatching(gIOCatalog, g_hid_cats, (drv_match_func_t)_matching_driver, self);

    mtx_lock(&self->mtx);

    for (;;) {
        self->report.type = kHIDReportType_Null;
        clock_gettime(g_mono_clock, &self->now);


        switch (signo) {
            case SIGVBL: {
                bool r = false;

                r |= _collect_keyboard_reports(self);
                r |= _collect_pointing_device_reports(self);
                r |= _collect_gamepad_reports(self);

                if (r) {
                    cnd_broadcast(&self->evqCnd);
                }
                break;
            }

            case SIGSCR:
                _collect_framebuffer_size(self);
                break;
        }


        mtx_unlock(&self->mtx);
        vcpu_sigwait(&self->reportsWaitQueue, &self->reportSigs, 0, TICKS_MAX, &signo);
        mtx_lock(&self->mtx);
    }

    mtx_unlock(&self->mtx);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Driver Interface
////////////////////////////////////////////////////////////////////////////////


// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
errno_t HIDDriver_read(HIDDriverRef _Nonnull self, unsigned int mode, off_t* _Nonnull pOffset, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    const bool isNonBlocking = (mode & O_NONBLOCK) == O_NONBLOCK;
    const nanotime_t* timp = (isNonBlocking) ? &NANOTIME_ZERO : &NANOTIME_INF;
    HIDEvent* pe = buf;
    ssize_t nBytesRead = 0;

    while ((nBytesRead + sizeof(HIDEvent)) <= nBytesToRead) {
        // Only block waiting for the first event. For all other events we do not
        // wait.
        const errno_t e1 = HIDDriver_GetNextEvent(self, (pe == buf) ? timp : &NANOTIME_ZERO, pe);

        if (e1 != EOK) {
            // Return with an error if we were not able to read any event data at
            // all and return with the data we were able to read otherwise.
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        nBytesRead += sizeof(HIDEvent);
        pe++;
    }

    *nOutBytesRead = nBytesRead;
    return err;
}

errno_t HIDDriver_ioctl(HIDDriverRef _Nonnull self, unsigned int mode, off_t* _Nonnull pOffset, int cmd, va_list ap)
{
    switch (cmd) {
        case kHIDCommand_GetNextEvent: {
            const nanotime_t* timeoutp = va_arg(ap, nanotime_t*);
            HIDEvent* evt = va_arg(ap, HIDEvent*);

            return HIDDriver_GetNextEvent(self, timeoutp, evt);
        }

        case kHIDCommand_FlushEvents:
            HIDDriver_FlushEvents(self);
            return EOK;
            
        case kHIDCommand_GetKeyRepeatDelays: {
            nanotime_t* initialp = va_arg(ap, nanotime_t*);
            nanotime_t* repeatp = va_arg(ap, nanotime_t*);

            HIDDriver_GetKeyRepeatDelays(self, initialp, repeatp);
            return EOK;
        }

        case kHIDCommand_SetKeyRepeatDelays: {
            const nanotime_t* initialp = va_arg(ap, nanotime_t*);
            const nanotime_t* repeatp = va_arg(ap, nanotime_t*);

            HIDDriver_SetKeyRepeatDelays(self, initialp, repeatp);
            return EOK;
        }

        case kHIDCommand_ObtainCursor: {
            return HIDDriver_ObtainCursor(self);
        }

        case kHIDCommand_ReleaseCursor:
            HIDDriver_ReleaseCursor(self);
            return EOK;

        case kHIDCommand_SetCursor: {
            const void** planes = va_arg(ap, const void**);
            const size_t bytesPerRow = va_arg(ap, size_t);
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const pixfmt_t format = va_arg(ap, pixfmt_t);
            const int hotSpotX = va_arg(ap, int);
            const int hotSpotY = va_arg(ap, int);

            return HIDDriver_SetCursor(self, planes, bytesPerRow, width, height, format, hotSpotX, hotSpotY);
        }

        case kHIDCommand_ShowCursor:
            HIDDriver_ShowCursor(self);
            return EOK;

        case kHIDCommand_HideCursor:
            HIDDriver_HideCursor(self);
            return EOK;

        case kHIDCommand_ObscureCursor:
            HIDDriver_ObscureCursor(self);
            return EOK;

        case kHIDCommand_ShieldCursor: {
            const int x = va_arg(ap, int);
            const int y = va_arg(ap, int);
            const int w = va_arg(ap, int);
            const int h = va_arg(ap, int);

            return HIDDriver_ShieldMouseCursor(self, x, y, w, h);
        }

        default:
            return super_n(ioctl, Driver, HIDDriver, self, mode, pOffset, cmd, ap);
    }
}


class_func_defs(HIDDriver, Driver,
override_func_def(onStart, HIDDriver, Driver)
override_func_def(read, HIDDriver, Driver)
override_func_def(ioctl, HIDDriver, Driver)
);
