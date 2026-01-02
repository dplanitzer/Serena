//
//  cnd_signal.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"
#include <errno.h>


static int __cnd_awake(cnd_t* _Nonnull self, int flags)
{
    if (self->signature != CND_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    wq_wakeup(self->wait_queue, flags);
    return 0;
}

int cnd_signal(cnd_t* _Nonnull self)
{
    return __cnd_awake(self, WAKE_ONE);
}

int cnd_broadcast(cnd_t* _Nonnull self)
{
    return __cnd_awake(self, WAKE_ALL);
}
