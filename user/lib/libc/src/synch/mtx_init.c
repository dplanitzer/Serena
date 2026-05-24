//
//  mtx_init.c
//  libc
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

errno_t mtx_init(mtx_t* _Nonnull self, int type)
{
    switch (type) {
        case mtx_plain:
        case mtx_timed:
            break;

        default:
            return EINVAL;
    }

    atomic_init(&self->state, 0);
    self->caps = type;
    
    return EOK;
}
