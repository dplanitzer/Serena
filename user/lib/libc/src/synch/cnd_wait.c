//
//  cnd_wait.c
//  libc
//
//  Based on: https://locklessinc.com/articles/mutex_cv_futex/
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

errno_t cnd_wait(cnd_t* _Nonnull self, mtx_t* _Nonnull mutex)
{
    const int seq = atomic_int_load(&self->seq);

    mtx_unlock(mutex);

    woa_wait(&self->seq, seq, 0, NULL);

    while (atomic_int_exchange(&mutex->state, _MTX_CONTENDED) != _MTX_AVAILABLE) {
        woa_wait(&mutex->state, _MTX_CONTENDED, 0, NULL);
    }

    return EOK;
}
