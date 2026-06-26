//
//  evs.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef hid_evs_h
#define hid_evs_h

#include <kpi/hid_event.h>


// Action to take next as the result of calling hid_evs_tickle()
typedef enum hid_action {
    HIDSynth_Wait,
    HIDSynth_TimedWait,
    HIDSynth_UseEvent,
    HIDSynth_MakeRepeat
} hid_action_t;

// Result of calling hid_evs_tickle()
typedef struct hid_evs_res {
    nanotime_t  deadline;   // MakeRepeat: event timestamp; TimedWait: wait until this time
    uint32_t    flags;      // MakeRepeat: event flags
    hid_key_t  keyCode;    // MakeRepeat: event key code
} hid_evs_res_t;


typedef struct hid_evs {
    nanotime_t  initialKeyRepeatDelay;        // [200ms...3s]
    nanotime_t  keyRepeatDelay;               // [20ms...2s]

    // At most one key may be in key repeat state
    nanotime_t  nextEventTime;
    uint32_t    keyFlags;       // Modifier flags of the original key down (flags changes cause the repeat to end)
    hid_key_t  keyCode;        // Key code of the original key down
    uint16_t    state;
} hid_evs_t;


extern void hid_evs_init(hid_evs_t* _Nonnull self);
extern void hid_evs_destroy(hid_evs_t* _Nullable self);

// Resets the event synthesizer to idle state.
extern void hid_evs_reset(hid_evs_t* _Nonnull self);

// This method looks at the current key repeat state and the current event 'evt'
// and it determines whether the caller should return 'evt' to the user,
// synthesize a key repeat event and return it to the user or whether it should
// wait for events to arrive.
extern hid_action_t hid_evs_tickle(hid_evs_t* _Nonnull self, const hid_event_t* _Nullable evt, hid_evs_res_t* _Nonnull result);

#endif /* hid_evs_h */
