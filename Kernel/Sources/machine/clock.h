//
//  clock.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CLOCK_H
#define _CLOCK_H 1

#include <kern/types.h>

// At most 1ms
#define CLOCK_DELAY_MAX_NSEC    1000000l


// Note: Keep in sync with machine/hal/lowmem.i
struct clock {
    volatile struct timespec    current_time;
    volatile Quantums           current_quantum;    // Current scheduler time in terms of elapsed quantums since boot
    int32_t                     ns_per_quantum;     // duration of a quantum in terms of nanoseconds
    int16_t                     quantum_duration_cycles;    // Quantum duration in terms of timer cycles
    int16_t                     ns_per_quantum_timer_cycle; // Length of a quantum timer cycle in nanoseconds
};
typedef struct clock* clock_ref_t;


extern clock_ref_t _Nonnull g_mono_clock;

// Initializes the monotonic clock. Note that the clock is stopped by default.
// Call clock_start() once the system is ready to run the clock and accept clock
// related interrupts.
extern void clock_init_mono(clock_ref_t _Nonnull self);

extern void clock_start(clock_ref_t _Nonnull self);

// Returns the current time in terms of quantums
#define clock_getticks(__self) \
((__self)->current_quantum)

// Returns the current time of the clock in terms of microseconds.
extern void clock_gettime(clock_ref_t _Nonnull self, struct timespec* _Nonnull ts);

// Blocks the caller for 'ns' nanoseconds. This functions blocks for at most
// CLOCK_DELAY_MAX_NSEC and the delay is implemented as a hard spin.
// Longer delays should be implemented with the help of a wait queue. 
extern void clock_delay(clock_ref_t _Nonnull self, long ns);


// Rounding modes for struct timespec to Quantums conversion
#define QUANTUM_ROUNDING_TOWARDS_ZERO   0
#define QUANTUM_ROUNDING_AWAY_FROM_ZERO 1

// Converts a timespec to a quantum value. The quantum value is rounded based
// on the 'rounding' parameter.
extern Quantums clock_time2quantums(clock_ref_t _Nonnull self, const struct timespec* _Nonnull ts, int rounding);

// Converts a quantum value to a timespec.
extern void clock_quantums2time(clock_ref_t _Nonnull self, Quantums quants, struct timespec* _Nonnull ts);

// @HAL Requirement: Must be called from the monotonic clock IRQ handler first
extern void clock_irq(void);

#endif /* _CLOCK_H */
