//
//  serena/sem_wait.c
//  libc
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

errno_t sem_wait(sem_t* _Nonnull self)
{
    int r = 0;

    for (;;) {
        if (__sem_trywait(self)) {
            break;
        }

        if (woa_wait(&self->value, 0, 0, NULL) != 0) {
            r = -1;
            break;
        }
    }

    return (r == 0) ? EOK : errno;
}
