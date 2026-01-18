//
//  cnd_deinit.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"
#include <errno.h>
#include <kpi/syscall.h>


int cnd_deinit(cnd_t* _Nonnull self)
{
    if (self->signature != CND_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = _syscall(SC_wq_dispose, self->wait_queue);
    self->signature = 0;
    self->wait_queue = -1;

    return 0;
}
