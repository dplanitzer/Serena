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

#define kTicks_Infinity     INT32_MAX
#define kTicks_Epoch        0


// Note: Keep in sync with machine/hal/lowmem.i
struct clock {
    volatile tick_t tick_count;         // Current scheduler time in terms of ticks quantums since clock start
    int32_t         ns_per_tick;        // duration of a clock tick in terms of nanoseconds
    int16_t         cia_cycles_per_tick;    // duration of a clock tick in terms of CIA chip cycles 
    int16_t         ns_per_cia_cycle;       // length of a CIA cycle in nanoseconds
};
typedef struct clock* clock_ref_t;


extern clock_ref_t _Nonnull g_mono_clock;

// Initializes the monotonic clock. Note that the clock is stopped by default.
// Call clock_start() once the system is ready to run the clock and accept clock
// related interrupts.
extern void clock_init_mono(clock_ref_t _Nonnull self);

extern void clock_start(clock_ref_t _Nonnull self);

// Returns the current time in terms of clock ticks
#define clock_getticks(__self) \
((__self)->tick_count)

// Returns the duration of a single clock tick in terms of seconds and nanoseconds.
#define clock_getresolution(__self, __res) \
(__res)->tv_sec = 0; \
(__res)->tv_nsec = (__self)->ns_per_tick

// Returns the current time of the clock in terms of the clock tick resolution.
extern void clock_gettime(clock_ref_t _Nonnull self, struct timespec* _Nonnull ts);

// Returns the current time of the clock with microseconds precision.
extern void clock_gettime_hires(clock_ref_t _Nonnull self, struct timespec* _Nonnull ts);


// Rounding modes for struct timespec to tick_t conversion
#define CLOCK_ROUND_TOWARDS_ZERO   0
#define CLOCK_ROUND_AWAY_FROM_ZERO 1

// Converts a timespec to a clock tick value. The clock ticks are rounded based
// on the 'rounding' parameter.
extern tick_t clock_time2ticks(clock_ref_t _Nonnull self, const struct timespec* _Nonnull ts, int rounding);

// Converts a clock tick value to a timespec.
extern void clock_ticks2time(clock_ref_t _Nonnull self, tick_t ticks, struct timespec* _Nonnull ts);

#endif /* _CLOCK_H */
