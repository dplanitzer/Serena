//
//  cnd_signal.c
//  libc
//
//  Based on: https://locklessinc.com/articles/mutex_cv_futex/
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

int cnd_broadcast(cnd_t* _Nonnull self)
{
    if (self->signature != CND_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    atomic_int_fetch_add(&self->seq, 1);
    woa_wakeup(&self->seq, WAKEUP_ALL);
}
