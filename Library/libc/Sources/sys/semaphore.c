//
//  sys/semaphore.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/semaphore.h>
#include <System/_syscall.h>

#define SEM_SIGNATURE 0x53454d41

// Must be sizeof(USemaphore) <= 16 
typedef struct USemaphore {
    int             od;
    unsigned int    signature;
    int             r2;
    int             r3;
} USemaphore;


errno_t sem_init(sem_t* _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    self->signature = 0;
    self->r2 = 0;
    self->r3 = 0;

    const errno_t err = _syscall(SC_sem_create, npermits, &self->od);
    if (err == EOK) {
        self->signature = SEM_SIGNATURE;
    }

    return err;
}

errno_t sem_deinit(sem_t* _Nonnull sema)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature != SEM_SIGNATURE) {
        return EINVAL;
    }

    const errno_t err = _syscall(SC_dispose, self->od);
    self->signature = 0;
    self->od = 0;

    return err;
}

errno_t sem_post(sem_t* _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEM_SIGNATURE) {
        return _syscall(SC_sem_post, self->od, npermits);
    }
    else {
        return EINVAL;
    }
}

errno_t sem_wait(sem_t* _Nonnull sema, int npermits, struct timespec deadline)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEM_SIGNATURE) {
        return _syscall(SC_sem_wait, self->od, npermits, deadline);
    }
    else {
        return EINVAL;
    }
}

errno_t sem_trywait(sem_t* _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEM_SIGNATURE) {
        return _syscall(SC_sem_trywait, self->od, npermits);
    }
    else {
        return EINVAL;
    }
}
