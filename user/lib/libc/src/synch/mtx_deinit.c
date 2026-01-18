//
//  mtx_deinit.c
//  libc
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"
#include <kpi/syscall.h>


int mtx_deinit(mtx_t* _Nonnull self)
{
    if (self->signature != MTX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = _syscall(SC_wq_dispose, self->wait_queue);
    self->signature = 0;
    self->wait_queue = -1;

    return r;
}
