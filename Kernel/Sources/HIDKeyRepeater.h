//
//  HIDKeyRepeater.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/10/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDKeyRepeater_h
#define HIDKeyRepeater_h

#include <klib/klib.h>
#include "EventDriver.h"

struct _HIDKeyRepeater;
typedef struct _HIDKeyRepeater* HIDKeyRepeaterRef;


extern errno_t HIDKeyRepeater_Create(EventDriverRef pEventDriver, HIDKeyRepeaterRef _Nullable * _Nonnull pOutRepeater);
extern void HIDKeyRepeater_Destroy(HIDKeyRepeaterRef _Nonnull pRepeater);

extern void HIDKeyRepeater_GetKeyRepeatDelays(HIDKeyRepeaterRef _Nonnull pRepeater, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay);
extern void HIDKeyRepeater_SetKeyRepeatDelays(HIDKeyRepeaterRef _Nonnull pRepeater, TimeInterval initialDelay, TimeInterval repeatDelay);

// Informs the key repeater that the user is now pressing down the key 'keyCode'.
// This implicitly cancels an ongoing key repeat of a different key. Note that at
// most one key can be repeated at any given time.
extern void HIDKeyRepeater_KeyDown(HIDKeyRepeaterRef _Nonnull pRepeater, HIDKeyCode keyCode);

// Informs the key repeater that the user has just released the key 'keyCode'.
// This cancels the key repeat for this key.
extern void HIDKeyRepeater_KeyUp(HIDKeyRepeaterRef _Nonnull pRepeater, HIDKeyCode keyCode);

// Gives the key repeater a chance to update its internal state. The key repeater
// generates and posts a new key down/repeat event if such an event is due.
extern void HIDKeyRepeater_Tick(HIDKeyRepeaterRef _Nonnull pRepeater);

#endif /* HIDKeyRepeater_h */
