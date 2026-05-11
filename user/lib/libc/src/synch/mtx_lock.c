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

static int _compare_exchange(volatile atomic_int* _Nonnull p, int expected, int desired)
{
    int* ep = &expected;

    atomic_int_compare_exchange_strong(p, ep, desired);
    return *ep;
}

int mtx_lock(mtx_t* _Nonnull self)
{
    if (self->signature != MTX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }


    int s = _compare_exchange(&self->state, _MTX_AVAILABLE, _MTX_LOCKED);
    if (s != _MTX_AVAILABLE) {
        if (s != _MTX_CONTENDED) {
            s = atomic_int_exchange(&self->state, _MTX_CONTENDED);
        }

        while (s != _MTX_AVAILABLE) {
            ww_wait(&self->state, _MTX_CONTENDED);
            s = atomic_int_exchange(&self->state, _MTX_CONTENDED);
        }
    }

    return 0;
}
