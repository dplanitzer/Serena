//
//  HIDKeyRepeater.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/10/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "HIDKeyRepeater.h"
#include "HIDManager.h"
#include <hal/MonotonicClock.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <kpi/hidkeycodes.h>
#include <kern/timespec.h>


enum {
    kState_Idle = 0,
    kState_InitialDelaying,
    kState_Repeating
};

typedef struct HIDKeyRepeater {
    int8_t          repeatersInUseCount;    // number of repeaters currently in use
    int8_t          reserved[3];
    struct timespec initialKeyRepeatDelay;        // [200ms...3s]
    struct timespec keyRepeatDelay;               // [20ms...2s]

    // At most one key may be in key repeat state
    struct timespec nextEventTime;
    HIDKeyCode      keyCode;
    uint16_t        state;
} HIDKeyRepeater;



// Allocates a key repeater object.
errno_t HIDKeyRepeater_Create(HIDKeyRepeaterRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    HIDKeyRepeater* self;
    
    try(kalloc_cleared(sizeof(HIDKeyRepeater), (void**) &self));
    self->repeatersInUseCount = 0;
    self->initialKeyRepeatDelay = timespec_from_ms(300);
    self->keyRepeatDelay = timespec_from_ms(100);
    self->state = kState_Idle;

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

// Frees the key repeater.
void HIDKeyRepeater_Destroy(HIDKeyRepeaterRef _Nonnull self)
{
    kfree(self);
}

void HIDKeyRepeater_GetKeyRepeatDelays(HIDKeyRepeaterRef _Nonnull self, struct timespec* _Nullable pInitialDelay, struct timespec* _Nullable pRepeatDelay)
{
    if (pInitialDelay) {
        *pInitialDelay = self->initialKeyRepeatDelay;
    }
    if (pRepeatDelay) {
        *pRepeatDelay = self->keyRepeatDelay;
    }
}

void HIDKeyRepeater_SetKeyRepeatDelays(HIDKeyRepeaterRef _Nonnull self, struct timespec initialDelay, struct timespec repeatDelay)
{
    self->initialKeyRepeatDelay = initialDelay;
    self->keyRepeatDelay = repeatDelay;
}

// Returns true if the given key should be auto-repeated
static bool shouldAutoRepeatKeyCode(HIDKeyCode keyCode)
{
    // Everything except:
    // - modifier keys
    // - caps lock
    // - tab, return, esc
    // - some function keys (print screen, etc)
    // - key controller messages (errors)
    switch (keyCode) {
        case KEY_LEFTCTRL:
        case KEY_LEFTSHIFT:
        case KEY_LEFTALT:
        case KEY_LEFTMETA:
        case KEY_RIGHTCTRL:
        case KEY_RIGHTSHIFT:
        case KEY_RIGHTALT:
        case KEY_RIGHTMETA:

        case KEY_CAPSLOCK:

        case KEY_TAB:
        case KEY_ENTER:
        case KEY_KPENTER:
        case KEY_ESC:

        case KEY_SYSRQ:
        case KEY_SCROLLLOCK:
        case KEY_NUMLOCK:
        case KEY_PAUSE:
        case KEY_INSERT:
        case KEY_POWER:
        case KEY_COMPOSE:
        case KEY_OPEN:
        case KEY_HELP:
        case KEY_PROPS:
        case KEY_FRONT:
        case KEY_STOP:
        case KEY_AGAIN:
        case KEY_UNDO:
        case KEY_CUT:
        case KEY_COPY:
        case KEY_PASTE:
        case KEY_FIND:
        case KEY_MUTE:

        case KEY_RO:
        case KEY_KATAKANAHIRAGANA:
        case KEY_YEN:
        case KEY_HENKAN:
        case KEY_MUHENKAN:
        case KEY_HANGEUL:
        case KEY_HANJA:
        case KEY_KATAKANA:
        case KEY_HIRAGANA:
        case KEY_ZENKAKUHANKAKU:

        case KEY_MEDIA_PLAYPAUSE:
        case KEY_MEDIA_STOPCD:
        case KEY_MEDIA_EJECTCD:
        case KEY_MEDIA_MUTE:
        case KEY_MEDIA_WWW:
        case KEY_MEDIA_STOP:
        case KEY_MEDIA_FIND:
        case KEY_MEDIA_EDIT:
        case KEY_MEDIA_SLEEP:
        case KEY_MEDIA_COFFEE:
        case KEY_MEDIA_REFRESH:
        case KEY_MEDIA_CALC: 

        case KEY_NONE:
        case KEY_ERR_OVF:
        case KEY_ERR_POST:
        case KEY_ERR_UNDEF:
            return false;
            
        default:
            return true;
    }
}

// Informs the key repeater that the user is now pressing down the key 'keyCode'.
// This implicitly cancels an ongoing key repeat of a different key. Note that at
// most one key can be repeated at any given time.
void HIDKeyRepeater_KeyDown(HIDKeyRepeaterRef _Nonnull self, HIDKeyCode keyCode)
{
    if (shouldAutoRepeatKeyCode(keyCode)) {
        self->state = kState_InitialDelaying;
        self->keyCode = keyCode;
        self->nextEventTime = timespec_add(MonotonicClock_GetCurrentTime(), self->initialKeyRepeatDelay);
    }
}

// Informs the key repeater that the user has just released the key 'keyCode'.
// This cancels the key repeat for this key.
void HIDKeyRepeater_KeyUp(HIDKeyRepeaterRef _Nonnull self, HIDKeyCode keyCode)
{
    if (self->state != kState_Idle && self->keyCode == keyCode) {
        self->state = kState_Idle;
    }
}

// Gives the key repeater a chance to update its internal state. The key repeater
// generates and posts a new key down/repeat event if such an event is due.
void HIDKeyRepeater_Tick(HIDKeyRepeaterRef _Nonnull self)
{
    const struct timespec now = MonotonicClock_GetCurrentTime();

    switch (self->state) {
        case kState_Idle:
            break;

        case kState_InitialDelaying:
            if (timespec_gteq(now, self->nextEventTime)) {
                self->state = kState_Repeating;
                HIDManager_ReportKeyboardDeviceChange(gHIDManager, kHIDKeyState_Repeat, self->keyCode);
                
                while (timespec_ls(self->nextEventTime, now)) {
                    self->nextEventTime = timespec_add(self->nextEventTime, self->keyRepeatDelay);
                }
            }
            break;

        case kState_Repeating:
            if (timespec_gteq(now, self->nextEventTime)) {
                HIDManager_ReportKeyboardDeviceChange(gHIDManager, kHIDKeyState_Repeat, self->keyCode);
                
                while (timespec_ls(self->nextEventTime, now)) {
                    self->nextEventTime = timespec_add(self->nextEventTime, self->keyRepeatDelay);
                }
            }
            break;

        default:
            abort();
    }
}
