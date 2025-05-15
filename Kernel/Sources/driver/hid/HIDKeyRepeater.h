//
//  HIDKeyRepeater.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/10/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDKeyRepeater_h
#define HIDKeyRepeater_h

#include <System/Error.h>
#include <sys/hidevent.h>

struct HIDKeyRepeater;
typedef struct HIDKeyRepeater* HIDKeyRepeaterRef;


extern errno_t HIDKeyRepeater_Create(HIDKeyRepeaterRef _Nullable * _Nonnull pOutSelf);
extern void HIDKeyRepeater_Destroy(HIDKeyRepeaterRef _Nonnull self);

extern void HIDKeyRepeater_GetKeyRepeatDelays(HIDKeyRepeaterRef _Nonnull self, struct timespec* _Nullable pInitialDelay, struct timespec* _Nullable pRepeatDelay);
extern void HIDKeyRepeater_SetKeyRepeatDelays(HIDKeyRepeaterRef _Nonnull self, struct timespec initialDelay, struct timespec repeatDelay);

// Informs the key repeater that the user is now pressing down the key 'keyCode'.
// This implicitly cancels an ongoing key repeat of a different key. Note that at
// most one key can be repeated at any given time.
extern void HIDKeyRepeater_KeyDown(HIDKeyRepeaterRef _Nonnull self, HIDKeyCode keyCode);

// Informs the key repeater that the user has just released the key 'keyCode'.
// This cancels the key repeat for this key.
extern void HIDKeyRepeater_KeyUp(HIDKeyRepeaterRef _Nonnull self, HIDKeyCode keyCode);

// Gives the key repeater a chance to update its internal state. The key repeater
// generates and posts a new key down/repeat event if such an event is due.
extern void HIDKeyRepeater_Tick(HIDKeyRepeaterRef _Nonnull self);

#endif /* HIDKeyRepeater_h */
