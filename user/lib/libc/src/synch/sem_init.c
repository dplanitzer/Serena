//
//  serena/sem_init.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

int sem_init(sem_t* _Nonnull self, int value)
{
    atomic_init(&self->value, value);
    self->signature = SEM_SIGNATURE;

    return 0;
}
