//
//  __mtx_unlock.c
//  libc
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"
#include <stdbool.h>


int __mtx_unlock(mtx_t* _Nonnull self)
{
    bool doWakeup = false;

    if (self->signature != MTX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    spin_lock(&self->spinlock);
    self->state = 0;

    if (self->waiters > 0) {
        doWakeup = true;
    }
    spin_unlock(&self->spinlock);

    return (doWakeup) ? 1 : 0;
}
