//
//  cnd_timedwait.c
//  libc
//
//  Based on: https://locklessinc.com/articles/mutex_cv_futex/
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

errno_t cnd_timedwait(cnd_t* _Nonnull self, mtx_t* _Nonnull mutex, int flags, const nanotime_t* _Nonnull wtp)
{
    const int seq = atomic_int_load(&self->seq);

    mtx_unlock(mutex);

    int r = woa_wait(&self->seq, seq, flags, wtp);

    // Note that we need to acquire the mutex even if the timed wait timed out.
    while (atomic_int_exchange(&mutex->state, _MTX_CONTENDED) != _MTX_AVAILABLE) {
        woa_wait(&mutex->state, _MTX_CONTENDED, 0, NULL);
    }

    return (r == 0) ? EOK : errno;
}
