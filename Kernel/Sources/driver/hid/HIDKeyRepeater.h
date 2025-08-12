//
//  HIDKeyRepeater.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/10/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDKeyRepeater_h
#define HIDKeyRepeater_h

#include <kpi/hidevent.h>


// Action to take next as the result of calling HIDKeyRepeater_Tick()
typedef enum HIDKeyTick {
    kHIDKeyTick_Wait,
    kHIDKeyTick_TimedWait,
    kHIDKeyTick_UseEvent,
    kHIDKeyTick_SynthesizeRepeat
} HIDKeyTick;

// Result of calling HIDKeyRepeater_Tick()
typedef struct HIDKeyTickResult {
    struct timespec deadline;   // SynthesizeEvent: event timestamp; TimedWait: wait until this time
    uint32_t        flags;      // SynthesizeEvent: event flags
    HIDKeyCode      keyCode;    // SynthesizeEvent: event key code
} HIDKeyTickResult;


typedef struct HIDKeyRepeater {
    struct timespec initialKeyRepeatDelay;        // [200ms...3s]
    struct timespec keyRepeatDelay;               // [20ms...2s]

    // At most one key may be in key repeat state
    struct timespec nextEventTime;
    uint32_t        keyFlags;       // Modifier flags of the original key down (flags changes cause the repeat to end)
    HIDKeyCode      keyCode;        // Key code of the original key down
    uint16_t        state;
} HIDKeyRepeater;
typedef HIDKeyRepeater* HIDKeyRepeaterRef;


extern void HIDKeyRepeater_Init(HIDKeyRepeaterRef _Nonnull self);
extern void HIDKeyRepeater_Deinit(HIDKeyRepeaterRef _Nullable self);

// This method looks at the current key repeat state and the current event 'evt'
// and it determines whether the caller should return 'evt' to the user,
// synthesize a key repeat event and return it to the user or whether it should
// wait for events to arrive.
extern HIDKeyTick HIDKeyRepeater_Tick(HIDKeyRepeaterRef _Nonnull self, const HIDEvent* _Nullable evt, HIDKeyTickResult* _Nonnull result);

#endif /* HIDKeyRepeater_h */
