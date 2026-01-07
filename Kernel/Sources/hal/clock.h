//
//  clock.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CLOCK_H
#define _CLOCK_H 1

#include <stdbool.h>
#include <stdint.h>
#include <kpi/types.h>

#define kTicks_Infinity     UINT32_MAX
#define kTicks_Epoch        0


typedef void (*deadline_func_t)(void* _Nullable arg);

// Note: Keep in sync with machine/hw/m68k/lowmem.i
typedef struct clock_deadline {
    struct clock_deadline* _Nullable    next;
    tick_t                              deadline;
    deadline_func_t _Nonnull            func;
    void* _Nullable                     arg;
    bool                                isArmed;
    char                                reserved[3];
} clock_deadline_t;

#define CLOCK_DEADLINE_INIT (clock_deadline_t){NULL, 0, NULL, NULL, false, 0, 0, 0}


// Note: Keep in sync with machine/hw/m68k/lowmem.i
struct clock {
    volatile tick_t             tick_count;         // Current scheduler time in terms of ticks quantums since clock start
    clock_deadline_t* _Nullable deadline_queue;
    int32_t                     ns_per_tick;        // duration of a clock tick in terms of nanoseconds
    int16_t                     cia_cycles_per_tick;    // duration of a clock tick in terms of CIA chip cycles 
    int16_t                     ns_per_cia_cycle;       // length of a CIA cycle in nanoseconds
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


// Converts a timespec to a clock tick value, applying truncation.
extern tick_t clock_time2ticks_floor(clock_ref_t _Nonnull self, const struct timespec* _Nonnull ts);

// Converts a timespec to a clock tick value by rounding fractional clock ticks
// to the next higher clock tick value.
extern tick_t clock_time2ticks_ceil(clock_ref_t _Nonnull self, const struct timespec* _Nonnull ts);

// Converts a clock tick value to a timespec.
extern void clock_ticks2time(clock_ref_t _Nonnull self, tick_t ticks, struct timespec* _Nonnull ts);


// Registers the deadline timer 'deadline' with the clock 'self'. The deadline
// field in the deadline structure must have been initialized to the absolute
// time when the deadline should fire. A deadline that has fired is automatically
// removed from the clock. The caller is responsible for keeping the deadline
// structure alive and valid until it has fired or is cancelled.
extern void clock_deadline(clock_ref_t _Nonnull self, clock_deadline_t* _Nonnull deadline);

// Cancels the deadline timer 'deadline'. This function preserves the current
// values of the 'deadline', 'func' and 'arg' fields. Returns true if the timer
// was armed and has been canceled; false if the timer was already canceled.
extern bool clock_cancel_deadline(clock_ref_t _Nonnull self, clock_deadline_t* _Nonnull deadline);

#endif /* _CLOCK_H */
