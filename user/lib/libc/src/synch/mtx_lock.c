//
//  mtx_lock.c
//  libc
//
//  Based on: https://www.akkadia.org/drepper/futex.pdf
// 
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

errno_t mtx_lock(mtx_t* _Nonnull self)
{
    int s = _MTX_AVAILABLE;
    atomic_int_compare_exchange_strong(&self->state, &s, _MTX_LOCKED);

    if (s != _MTX_AVAILABLE) {
        if (s != _MTX_CONTENDED) {
            s = atomic_int_exchange(&self->state, _MTX_CONTENDED);
        }

        while (s != _MTX_AVAILABLE) {
            woa_wait(&self->state, _MTX_CONTENDED, 0, NULL);
            s = atomic_int_exchange(&self->state, _MTX_CONTENDED);
        }
    }

    return EOK;
}
