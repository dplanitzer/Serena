//
//  cnd_init.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"


int cnd_init(cnd_t* _Nonnull self)
{
    self->spinlock = SPINLOCK_INIT;
    self->signature = CND_SIGNATURE;

    self->wait_queue = wq_create(WAITQUEUE_FIFO);
    if (self->wait_queue < 0) {
        self->signature = 0;
        return -1;
    }

    return 0;
}
