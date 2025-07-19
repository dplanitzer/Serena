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

// At most 1ms
#define MONOTONIC_DELAY_MAX_NSEC    1000000l

#define INTERRUPT_ID_QUANTUM_TIMER  INTERRUPT_ID_CIA_A_TIMER_B


// Note: Keep in sync with machine/hal/lowmem.i
typedef struct MonotonicClock {
    volatile struct timespec    current_time;
    volatile Quantums           current_quantum;    // Current scheduler time in terms of elapsed quantums since boot
    int32_t                     ns_per_quantum;     // duration of a quantum in terms of nanoseconds
    int16_t                     quantum_duration_cycles;    // Quantum duration in terms of timer cycles
    int16_t                     ns_per_quantum_timer_cycle; // Length of a quantum timer cycle in nanoseconds
} MonotonicClock;


extern MonotonicClock* _Nonnull gMonotonicClock;

extern errno_t MonotonicClock_CreateForLocalCPU(const struct SystemDescription* pSysDesc);

// Returns the current time in terms of quantums
extern Quantums MonotonicClock_GetCurrentQuantums(void);

// Returns the current time of the clock in terms of microseconds.
extern void MonotonicClock_GetCurrentTime(struct timespec* _Nonnull ts);

// Blocks the caller for 'ns' nanoseconds. This functions blocks for at most
// MONOTONIC_DELAY_MAX_NSEC and the delay is implemented as a hard spin.
// Longer delays should be implemented with the help of a wait queue. 
extern void MonotonicClock_Delay(long ns);


// Rounding modes for struct timespec to Quantums conversion
#define QUANTUM_ROUNDING_TOWARDS_ZERO   0
#define QUANTUM_ROUNDING_AWAY_FROM_ZERO 1

// Converts a timespec to a quantum value. The quantum value is rounded based
// on the 'rounding' parameter.
extern Quantums Quantums_MakeFromTimespec(const struct timespec* _Nonnull ts, int rounding);

// Converts a quantum value to a timespec.
extern void Timespec_MakeFromQuantums(struct timespec* _Nonnull ts, Quantums quants);

#endif /* MonotonicClock_h */
