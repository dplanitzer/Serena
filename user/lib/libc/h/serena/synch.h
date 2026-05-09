//
//  serena/synch.h
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_SYNCH_H
#define _SERENA_SYNCH_H 1

#include <_cmndef.h>
#include <ext/atomic.h>
#include <ext/nanotime.h>
#include <kpi/types.h>
#include <kpi/synch.h>

__CPP_BEGIN

// Blocks the caller as long as the integer-sized memory cell at address 'addr'
// contains the value 'expected' and no ww_wakeup() call has been issued on the
// same address.
extern int ww_wait(volatile atomic_int* _Nonnull addr, int expected);

// Same as wq_wait() but allows you to specify a timeout. The timeout is a
// duration by default. Pass TIMER_ABSTIME in 'flags' to make it an absolute
// time value. Returns ETIMEDOUT if the timeout is reached.
extern int ww_timedwait(volatile atomic_int* _Nonnull addr, int expected, int flags, const nanotime_t* _Nonnull wtp);

// Wakes up one or all waiters currently blocked on the address 'addr'.
extern int ww_wakeup(volatile atomic_int* _Nonnull addr, int flags);

__CPP_END

#endif /* _SERENA_SYNCH_H */
