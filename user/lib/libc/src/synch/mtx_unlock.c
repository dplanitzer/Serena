//
//  mtx_unlock.c
//  libc
//
//  Based on: https://www.akkadia.org/drepper/futex.pdf
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

int mtx_unlock(mtx_t* _Nonnull self)
{
    if (self->signature != MTX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    if (atomic_int_fetch_sub(&self->state, 1) != _MTX_LOCKED) {
        atomic_int_store(&self->state, _MTX_AVAILABLE);
        woa_wakeup(&self->state, WAKEUP_ONE);
    }
    return 0;
}
