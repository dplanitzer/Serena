//
//  mtx_init.c
//  libc
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

errno_t mtx_init(mtx_t* _Nonnull self)
{
    atomic_init(&self->state, 0);
    return EOK;
}
