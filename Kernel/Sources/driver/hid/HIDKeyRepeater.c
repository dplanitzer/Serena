//
//  HIDKeyRepeater.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/10/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "HIDKeyRepeater.h"
#include <machine/clock.h>
#include <kern/kernlib.h>
#include <kpi/hidkeycodes.h>
#include <kern/timespec.h>


enum {
    kState_Idle = 0,
    kState_Repeating,
};


void HIDKeyRepeater_Init(HIDKeyRepeaterRef _Nonnull self)
{
    timespec_from_ms(&self->initialKeyRepeatDelay, 300);
    timespec_from_ms(&self->keyRepeatDelay, 100);
    self->state = kState_Idle;
}

void HIDKeyRepeater_Deinit(HIDKeyRepeaterRef _Nullable self)
{
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
static bool _key_repeat_tick(HIDKeyRepeaterRef _Nonnull self, HIDKeyTickResult* _Nonnull result)
{
    struct timespec now;

    clock_gettime(g_mono_clock, &now);

    if (timespec_ge(&now, &self->nextEventTime)) {
        result->deadline = self->nextEventTime;
        result->flags = self->keyFlags;
        result->keyCode = self->keyCode;

        while (timespec_lt(&self->nextEventTime, &now)) {
            timespec_add(&self->nextEventTime, &self->keyRepeatDelay, &self->nextEventTime);
        }

        return true;
    }
    else {
        return false;
    }
}

HIDKeyTick HIDKeyRepeater_Tick(HIDKeyRepeaterRef _Nonnull self, const HIDEvent* _Nullable evt, HIDKeyTickResult* _Nonnull result)
{
    const int evtType = (evt) ? evt->type : -1;
    struct timespec now;
    int r;

    switch (evtType) {
        case kHIDEventType_KeyDown:
            if (shouldAutoRepeatKeyCode(evt->data.key.keyCode)) {
                self->state = kState_Repeating;
                self->keyFlags = evt->data.key.flags;
                self->keyCode = evt->data.key.keyCode;
                clock_gettime(g_mono_clock, &now);
                timespec_add(&now, &self->initialKeyRepeatDelay, &self->nextEventTime);
            }
            else {
                self->state = kState_Idle;
            }
            r = kHIDKeyTick_UseEvent;
            break;

        case kHIDEventType_KeyUp:
            if (self->state == kState_Repeating && self->keyCode == evt->data.key.keyCode) {
                self->state = kState_Idle;
            }
            r = kHIDKeyTick_UseEvent;
            break;

        case kHIDEventType_FlagsChanged:
            if (self->state == kState_Repeating && self->keyFlags != evt->data.flags.flags) {
                self->state = kState_Idle;
            }
            r = kHIDKeyTick_UseEvent;
            break;

        default:    // All other event types or null event type
            if (self->state == kState_Repeating) {
                if (_key_repeat_tick(self, result)) {
                    r = (evtType == -1 || timespec_lt(&result->deadline, &evt->eventTime)) ? kHIDKeyTick_SynthesizeRepeat : kHIDKeyTick_UseEvent;
                }
                else {
                    if (evtType >= 0) {
                        r = kHIDKeyTick_UseEvent;
                    }
                    else {
                        result->deadline = self->nextEventTime;
                        r = kHIDKeyTick_TimedWait;
                    }
                }
            }
            else {
                r = (evtType >= 0) ? kHIDKeyTick_UseEvent : kHIDKeyTick_Wait;
            }
            break;
    }

    return r;
}
