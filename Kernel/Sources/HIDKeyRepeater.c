//
//  HIDKeyRepeater.c
//  Apollo
//
//  Created by Dietmar Planitzer on 10/10/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "HIDKeyRepeater.h"
#include "USBHIDKeys.h"
#include "MonotonicClock.h"


enum {
    kState_Idle = 0,
    kState_InitialDelaying,
    kState_Repeating
};

typedef struct _HIDKeyRepeater {
    EventDriverRef  eventDriver;
    Int8            repeatersInUseCount;    // number of repeaters currently in use
    Int8            reserved[3];
    TimeInterval    initialKeyRepeatDelay;        // [200ms...3s]
    TimeInterval    keyRepeatDelay;               // [20ms...2s]

    // At most one key may be in key repeat state
    TimeInterval    nextEventTime;
    HIDKeyCode      keyCode;
    UInt16          state;
} HIDKeyRepeater;



// Allocates a key repeater object.
ErrorCode HIDKeyRepeater_Create(EventDriverRef pEventDriver, HIDKeyRepeaterRef _Nullable * _Nonnull pOutRepeater)
{
    decl_try_err();
    HIDKeyRepeater* pRepeater;
    
    try(kalloc_cleared(sizeof(HIDKeyRepeater), (void**) &pRepeater));
    pRepeater->eventDriver = pEventDriver;
    pRepeater->repeatersInUseCount = 0;
    pRepeater->initialKeyRepeatDelay = TimeInterval_MakeMilliseconds(300);
    pRepeater->keyRepeatDelay = TimeInterval_MakeMilliseconds(100);
    pRepeater->state = kState_Idle;

    *pOutRepeater = pRepeater;
    return EOK;

catch:
    *pOutRepeater = NULL;
    return err;
}

// Frees the key repeater.
void HIDKeyRepeater_Destroy(HIDKeyRepeaterRef _Nonnull pRepeater)
{
    if (pRepeater) {
        pRepeater->eventDriver = NULL;
        kfree(pRepeater);
    }
}

void HIDKeyRepeater_GetKeyRepeatDelays(HIDKeyRepeaterRef _Nonnull pRepeater, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay)
{
    if (pInitialDelay) {
        *pInitialDelay = pRepeater->initialKeyRepeatDelay;
    }
    if (pRepeatDelay) {
        *pRepeatDelay = pRepeater->keyRepeatDelay;
    }
}

void HIDKeyRepeater_SetKeyRepeatDelays(HIDKeyRepeaterRef _Nonnull pRepeater, TimeInterval initialDelay, TimeInterval repeatDelay)
{
    pRepeater->initialKeyRepeatDelay = initialDelay;
    pRepeater->keyRepeatDelay = repeatDelay;
}

// Returns true if the given key should be auto-repeated
static Bool shouldAutoRepeatKeyCode(HIDKeyCode keyCode)
{
    // Everything except:
    // - modifier keys
    // - caps lock
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
        case KEY_SYSRQ:
        case KEY_SCROLLLOCK:
        case KEY_NUMLOCK:
        case KEY_POWER:
            
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
void HIDKeyRepeater_KeyDown(HIDKeyRepeaterRef _Nonnull pRepeater, HIDKeyCode keyCode)
{
    if (shouldAutoRepeatKeyCode(keyCode)) {
        pRepeater->state = kState_InitialDelaying;
        pRepeater->keyCode = keyCode;
        pRepeater->nextEventTime = TimeInterval_Add(MonotonicClock_GetCurrentTime(), pRepeater->initialKeyRepeatDelay);
    }
}

// Informs the key repeater that the user has just released the key 'keyCode'.
// This cancels the key repeat for this key.
void HIDKeyRepeater_KeyUp(HIDKeyRepeaterRef _Nonnull pRepeater, HIDKeyCode keyCode)
{
    if (pRepeater->state != kState_Idle && pRepeater->keyCode == keyCode) {
        pRepeater->state = kState_Idle;
    }
}

// Gives the key repeater a chance to update its internal state. The key repeater
// generates and posts a new key down/repeat event if such an event is due.
void HIDKeyRepeater_Tick(HIDKeyRepeaterRef _Nonnull pRepeater)
{
    const TimeInterval now = MonotonicClock_GetCurrentTime();

    switch (pRepeater->state) {
        case kState_Idle:
            break;

        case kState_InitialDelaying:
            if (TimeInterval_GreaterEquals(now, pRepeater->nextEventTime)) {
                pRepeater->state = kState_Repeating;
                EventDriver_ReportKeyboardDeviceChange(pRepeater->eventDriver, kHIDKeyState_Repeat, pRepeater->keyCode);
                
                while (TimeInterval_Less(pRepeater->nextEventTime, now)) {
                    pRepeater->nextEventTime = TimeInterval_Add(pRepeater->nextEventTime, pRepeater->keyRepeatDelay);
                }
            }
            break;

        case kState_Repeating:
            if (TimeInterval_GreaterEquals(now, pRepeater->nextEventTime)) {
                EventDriver_ReportKeyboardDeviceChange(pRepeater->eventDriver, kHIDKeyState_Repeat, pRepeater->keyCode);
                
                while (TimeInterval_Less(pRepeater->nextEventTime, now)) {
                    pRepeater->nextEventTime = TimeInterval_Add(pRepeater->nextEventTime, pRepeater->keyRepeatDelay);
                }
            }
            break;

        default:
            abort();
    }
}
