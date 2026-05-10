//
//  serena/sem_post.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

int sem_post(sem_t* _Nonnull self)
{
    if (self->signature != SEM_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    atomic_int_fetch_add(&self->value, 1);
    ww_wakeup(&self->value, WAKEUP_ONE);
    
    return 0;
}
