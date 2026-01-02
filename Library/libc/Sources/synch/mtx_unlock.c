//
//  mtx_unlock.c
//  libc
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"


int mtx_unlock(mtx_t* _Nonnull self)
{
    const int r = __mtx_unlock(self);

    if (r == 1) {
        wq_wakeup(self->wait_queue, WAKE_ONE);
        return 0;
    }
    else {
        return r;
    }
}
