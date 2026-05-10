//
//  serena/sem_deinit.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

int sem_deinit(sem_t* _Nonnull self)
{
    if (self->signature != SEM_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    self->signature = 0;
    return 0;
}
