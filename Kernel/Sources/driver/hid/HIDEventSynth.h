//
//  HIDEventSynth.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDEventSynth_h
#define HIDEventSynth_h

#include <kpi/hidevent.h>


// Action to take next as the result of calling HIDEventSynth_Tick()
typedef enum HIDSynthAction {
    HIDSynth_Wait,
    HIDSynth_TimedWait,
    HIDSynth_UseEvent,
    HIDSynth_MakeRepeat
} HIDSynthAction;

// Result of calling HIDEventSynth_Tick()
typedef struct HIDSynthResult {
    struct timespec deadline;   // MakeRepeat: event timestamp; TimedWait: wait until this time
    uint32_t        flags;      // MakeRepeat: event flags
    HIDKeyCode      keyCode;    // MakeRepeat: event key code
} HIDSynthResult;


typedef struct HIDEventSynth {
    struct timespec initialKeyRepeatDelay;        // [200ms...3s]
    struct timespec keyRepeatDelay;               // [20ms...2s]

    // At most one key may be in key repeat state
    struct timespec nextEventTime;
    uint32_t        keyFlags;       // Modifier flags of the original key down (flags changes cause the repeat to end)
    HIDKeyCode      keyCode;        // Key code of the original key down
    uint16_t        state;
} HIDEventSynth;
typedef HIDEventSynth* HIDEventSynthRef;


extern void HIDEventSynth_Init(HIDEventSynthRef _Nonnull self);
extern void HIDEventSynth_Deinit(HIDEventSynthRef _Nullable self);

// This method looks at the current key repeat state and the current event 'evt'
// and it determines whether the caller should return 'evt' to the user,
// synthesize a key repeat event and return it to the user or whether it should
// wait for events to arrive.
extern HIDSynthAction HIDEventSynth_Tick(HIDEventSynthRef _Nonnull self, const HIDEvent* _Nullable evt, HIDSynthResult* _Nonnull result);

#endif /* HIDEventSynth_h */
