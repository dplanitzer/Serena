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

errno_t mtx_unlock(mtx_t* _Nonnull self)
{
    if (atomic_int_fetch_sub(&self->state, 1) != _MTX_LOCKED) {
#if defined(__M68K__)
        // CPU store instruction provides enough intrinsic atomicity that we can
        // do the write without the help of an atomic_store() call.
        self->state.value = _MTX_AVAILABLE;
#else
        atomic_int_store(&self->state, _MTX_AVAILABLE);
#endif
        woa_wakeup(&self->state, WAKEUP_ONE);
    }
    return EOK;
}
