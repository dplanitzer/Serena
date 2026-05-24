//
//  mtx_trylock.c
//  libc
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

errno_t mtx_trylock(mtx_t* _Nonnull self)
{
    int s = _MTX_AVAILABLE;
    atomic_int_compare_exchange_strong(&self->state, &s, _MTX_LOCKED);
    
    return (s == _MTX_AVAILABLE) ? EOK : EAGAIN;
}
