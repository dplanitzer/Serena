//
//  Semaphore.c
//  libsystem
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Semaphore.h>
#include <System/_syscall.h>

#define SEM_SIGNATURE 0x53454d41

// Must be sizeof(USemaphore) <= 16 
typedef struct USemaphore {
    int             od;
    unsigned int    signature;
    int             r2;
    int             r3;
} USemaphore;


errno_t os_sem_init(os_sem_t* _Nonnull sema, int npermits)
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

errno_t os_sem_deinit(os_sem_t* _Nonnull sema)
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

errno_t os_sem_post(os_sem_t* _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEM_SIGNATURE) {
        return _syscall(SC_sem_post, self->od, npermits);
    }
    else {
        return EINVAL;
    }
}

errno_t os_sem_wait(os_sem_t* _Nonnull sema, int npermits, TimeInterval deadline)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEM_SIGNATURE) {
        return _syscall(SC_sem_wait, self->od, npermits, deadline);
    }
    else {
        return EINVAL;
    }
}

errno_t os_sem_trywait(os_sem_t* _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEM_SIGNATURE) {
        return _syscall(SC_sem_trywait, self->od, npermits);
    }
    else {
        return EINVAL;
    }
}
