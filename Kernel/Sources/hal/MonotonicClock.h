//
//  MonotonicClock.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef MonotonicClock_h
#define MonotonicClock_h

#include <kern/errno.h>
#include <kern/types.h>

struct SystemDescription;


// Note: Keep in sync with lowmem.i
typedef struct MonotonicClock {
    volatile struct timespec    current_time;
    volatile Quantums           current_quantum;    // Current scheduler time in terms of elapsed quantums since boot
    int32_t                     ns_per_quantum;     // duration of a quantum in terms of nanoseconds
} MonotonicClock;


extern MonotonicClock* _Nonnull gMonotonicClock;

extern errno_t MonotonicClock_CreateForLocalCPU(const struct SystemDescription* pSysDesc);

// Returns the current time in terms of quantums
extern Quantums MonotonicClock_GetCurrentQuantums(void);

// Returns the current time of the clock in terms of microseconds.
extern void MonotonicClock_GetCurrentTime(struct timespec* _Nonnull ts);

// Blocks the caller until 'deadline'. Returns true if the function did the
// necessary delay and false if the caller should do something else instead to
// achieve the desired delay. Eg context switch to another virtual processor.
// Note that this function is only willing to block the caller for at most a few
// milliseconds. Longer delays should be done via a scheduler wait().
extern bool MonotonicClock_DelayUntil(const struct timespec* _Nonnull deadline);


// Rounding modes for struct timespec to Quantums conversion
#define QUANTUM_ROUNDING_TOWARDS_ZERO   0
#define QUANTUM_ROUNDING_AWAY_FROM_ZERO 1

// Converts a timespec to a quantum value. The quantum value is rounded based
// on the 'rounding' parameter.
extern Quantums Quantums_MakeFromTimespec(struct timespec* _Nonnull ts, int rounding);

// Converts a quantum value to a timespec.
extern void Timespec_MakeFromQuantums(struct timespec* _Nonnull ts, Quantums quants);

#endif /* MonotonicClock_h */
