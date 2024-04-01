//
//  MonotonicClock.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef MonotonicClock_h
#define MonotonicClock_h

#include <klib/klib.h>
#include <hal/SystemDescription.h>


typedef int32_t Quantums;             // Time unit of the scheduler clock which increments monotonically and once per quantum interrupt

// Note: Keep in sync with lowmem.i
typedef struct MonotonicClock {
    volatile TimeInterval   current_time;
    volatile Quantums       current_quantum;    // Current scheduler time in terms of elapsed quantums since boot
    int32_t                 ns_per_quantum;     // duration of a quantum in terms of nanoseconds
} MonotonicClock;


extern MonotonicClock* _Nonnull gMonotonicClock;

extern errno_t MonotonicClock_CreateForLocalCPU(const SystemDescription* pSysDesc);

extern Quantums MonotonicClock_GetCurrentQuantums(void);
extern TimeInterval MonotonicClock_GetCurrentTime(void);

// Blocks the caller until 'deadline'. Returns true if the function did the
// necessary delay and false if the caller should do something else instead to
// achieve the desired delay. Eg context switch to another virtual processor.
// Note that this function is only willing to block the caller for at most a few
// milliseconds. Longer delays should be done via a scheduler wait().
extern bool MonotonicClock_DelayUntil(TimeInterval deadline);


// Rounding modes for TimeInterval to Quantums conversion
#define QUANTUM_ROUNDING_TOWARDS_ZERO   0
#define QUANTUM_ROUNDING_AWAY_FROM_ZERO 1

// Converts a time interval to a quantum value. The quantum value is rounded
// based on the 'rounding' parameter.
extern Quantums Quantums_MakeFromTimeInterval(TimeInterval ti, int rounding);

// Converts a quantum value to a time interval.
extern TimeInterval TimeInterval_MakeFromQuantums(Quantums quants);

#endif /* MonotonicClock_h */
