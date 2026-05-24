//
//  serena/sem_timedwait.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"
#include <serena/clock.h>

errno_t sem_timedwait(sem_t* _Nonnull self, int flags, const nanotime_t* _Nonnull wtp)
{
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


    for (;;) {
        if (__sem_trywait(self)) {
            break;
        }

        if (woa_wait(&self->value, 0, flags, &a_timeout) != 0) {
            r = -1;
            break;
        }
    }

    return (r == 0) ? EOK : errno;
}
