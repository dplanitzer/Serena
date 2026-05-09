//
//  serena/sem_trywait.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"
#include <errno.h>


bool __sem_trywait(sem_t* _Nonnull self)
{
    // We spin here until we can execute the following operations in a consistent
    // and correct way:
    // - get the current semaphore value
    // - decrement it by one, if the value is > 0, and write it back to the semaphore store
    // we must be able to execute these operations atomically without the semaphore
    // value changing on us unexpectedly
    for (;;) {
        int curval = atomic_int_load(&self->value);

        if (curval <= 0) {
            return false;
        }

        if (atomic_int_compare_exchange_strong(&self->value, &curval, curval - 1)) {
            return true;
        }
    }

    /* NOT REACHED */
}

int sem_trywait(sem_t* _Nonnull self)
{
    if (self->signature != SEM_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    if (__sem_trywait(self)) {
        return 0;
    }
    else {
        errno = EAGAIN;
        return -1;
    }
}
