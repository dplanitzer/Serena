//
//  serena/sem_post.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

errno_t sem_post(sem_t* _Nonnull self)
{
    atomic_int_fetch_add(&self->value, 1);
    woa_wakeup(&self->value, WAKEUP_ONE);
    
    return EOK;
}
