//
//  sys/semaphore.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/semaphore.h>
#include <errno.h>
#include <kpi/syscall.h>

#define SEM_SIGNATURE 0x53454d41

// Must be sizeof(USemaphore) <= 16 
typedef struct USemaphore {
    int             od;
    unsigned int    signature;
    int             r2;
    int             r3;
} USemaphore;


int sem_init(sem_t* _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    self->signature = 0;
    self->r2 = 0;
    self->r3 = 0;

    if (_syscall(SC_sem_create, npermits, &self->od) == 0) {
        self->signature = SEM_SIGNATURE;
        return 0;
    }
    else {
        return -1;
    }
}

int sem_deinit(sem_t* _Nonnull sema)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature != SEM_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = _syscall(SC_dispose, self->od);
    self->signature = 0;
    self->od = 0;

    return r;
}

int sem_post(sem_t* _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEM_SIGNATURE) {
        return _syscall(SC_sem_post, self->od, npermits);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}

int sem_wait(sem_t* _Nonnull sema, int npermits, const struct timespec* _Nonnull deadline)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEM_SIGNATURE) {
        return _syscall(SC_sem_wait, self->od, npermits, deadline);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}

int sem_trywait(sem_t* _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEM_SIGNATURE) {
        return _syscall(SC_sem_trywait, self->od, npermits);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}
