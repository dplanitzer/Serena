//
//  mtx_init.c
//  libc
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"


int mtx_init(mtx_t* _Nonnull self)
{
    self->spinlock = SPINLOCK_INIT;
    self->state = 0;
    self->waiters = 0;
    self->signature = MTX_SIGNATURE;
    self->wait_queue = wq_create(WAITQUEUE_FIFO);

    if (self->wait_queue >= 0) {
        return 0;
    }
    else {
        self->signature = 0;
        return -1;
    }
}
