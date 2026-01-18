//
//  mtx_lock.c
//  libc
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"
#include <stdbool.h>


int mtx_lock(mtx_t* _Nonnull self)
{
    bool didWakeup = false;

    if (self->signature != MTX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    for (;;) {
        spin_lock(&self->spinlock);
        if (didWakeup) {
            self->waiters--;
        }

        if (self->state == 0) {
            self->state = 1;
            spin_unlock(&self->spinlock);
            return 0;
        }

        self->waiters++;
        spin_unlock(&self->spinlock);
        wq_wait(self->wait_queue);
        didWakeup = true;
    }
    /* NOT REACHED */
}
