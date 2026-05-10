//
//  mtx_trylock.c
//  libc
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

int mtx_trylock(mtx_t* _Nonnull self)
{
    if (self->signature != MTX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    int s = _MTX_AVAILABLE;
    atomic_int_compare_exchange_strong(&self->state, &s, _MTX_LOCKED);
    
    if (s == _MTX_AVAILABLE) {
        return 0;
    }
    else {
        errno = EAGAIN;
        return -1;
    }
}
