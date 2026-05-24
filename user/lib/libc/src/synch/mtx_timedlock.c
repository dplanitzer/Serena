//
//  mtx_timedlock.c
//  libc
//
//  Based on: https://www.akkadia.org/drepper/futex.pdf
// 
//  Created by Dietmar Planitzer on 5/23/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"
#include <serena/clock.h>

errno_t mtx_timedlock(mtx_t* _Nonnull self, int flags, const nanotime_t* _Nonnull wtp)
{
    int r = 0;
    int s = _MTX_AVAILABLE;
    atomic_int_compare_exchange_strong(&self->state, &s, _MTX_LOCKED);

    if (s != _MTX_AVAILABLE) {
        int r = 0;
        nanotime_t now, a_timeout;

        if ((flags & TIMER_ABSTIME) == TIMER_ABSTIME) {
            a_timeout = *wtp;
        }
        else {
            clock_time(CLOCK_MONOTONIC, &now);
            nanotime_add(&a_timeout, &now, wtp);
            flags |= TIMER_ABSTIME;
        }


        if (s != _MTX_CONTENDED) {
            s = atomic_int_exchange(&self->state, _MTX_CONTENDED);
        }

        while (s != _MTX_AVAILABLE) {
            if (woa_wait(&self->state, _MTX_CONTENDED, flags, &a_timeout) != 0) {
                r = -1;
                break;
            }
            s = atomic_int_exchange(&self->state, _MTX_CONTENDED);
        }
    }

    return (r == 0) ? EOK : errno;
}
