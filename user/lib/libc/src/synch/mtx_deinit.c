//
//  mtx_deinit.c
//  libc
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

int mtx_deinit(mtx_t* _Nonnull self)
{
    if (self->signature != MTX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    self->signature = 0;
    return 0;
}
