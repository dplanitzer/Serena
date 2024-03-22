//
//  Semaphore.c
//  libsystem
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Semaphore.h>
#include <System/_syscall.h>

#define SEMA_SIGNATURE 0x53454d41

// Must be sizeof(USemaphore) <= 16 
typedef struct USemaphore {
    int             od;
    unsigned int    signature;
    int             r2;
    int             r3;
} USemaphore;


errno_t Semaphore_Init(SemaphoreRef _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    self->signature = 0;
    self->r2 = 0;
    self->r3 = 0;

    const errno_t err = _syscall(SC_sema_create, npermits, &self->od);
    if (err == EOK) {
        self->signature = SEMA_SIGNATURE;
    }

    return err;
}

errno_t Semaphore_Deinit(SemaphoreRef _Nonnull sema)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature != SEMA_SIGNATURE) {
        return EINVAL;
    }

    const errno_t err = _syscall(SC_dispose, self->od);
    self->signature = 0;
    self->od = 0;

    return err;
}

errno_t Semaphore_Relinquish(SemaphoreRef _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEMA_SIGNATURE) {
        return _syscall(SC_sema_relinquish, self->od, npermits);
    }
    else {
        return EINVAL;
    }
}

errno_t Semaphore_Acquire(SemaphoreRef _Nonnull sema, int npermits, TimeInterval deadline)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEMA_SIGNATURE) {
        return _syscall(SC_sema_acquire, self->od, npermits, deadline);
    }
    else {
        return EINVAL;
    }
}

errno_t Semaphore_TryAcquire(SemaphoreRef _Nonnull sema, int npermits)
{
    USemaphore* self = (USemaphore*)sema;

    if (self->signature == SEMA_SIGNATURE) {
        return _syscall(SC_sema_tryacquire, self->od, npermits);
    }
    else {
        return EINVAL;
    }
}
