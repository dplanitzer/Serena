//
//  mtx_trylock.c
//  libc
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"


int mtx_trylock(mtx_t* _Nonnull self)
{
    if (self->signature != MTX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    spin_lock(&self->spinlock);
    if (self->state == 0) {
        self->state = 1;
        spin_unlock(&self->spinlock);
        return 0;
    }
    spin_unlock(&self->spinlock);

    errno = EBUSY;
    return -1;
}
