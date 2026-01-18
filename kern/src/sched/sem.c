//
//  sem.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "sem.h"
#include <assert.h>


// Initializes a new semaphore with 'value' permits
void sem_init(sem_t* _Nonnull self, int value)
{
    self->value = value;
    wq_init(&self->wq);
}

// Deinitializes the semaphore.
void sem_deinit(sem_t* _Nonnull self)
{
    assert(wq_deinit(&self->wq) == EOK);
}

// Invoked by sem_acquire() if the semaphore doesn't have the expected
// number of permits.
// @Entry Condition: preemption disabled
errno_t sem_onwait(sem_t* _Nonnull self, const struct timespec* _Nonnull deadline)
{
    return wq_timedwait(&self->wq,
                        NULL,
                        WAIT_ABSTIME,
                        deadline,
                        NULL);
}

// Invoked by sem_relinquish().
// @Entry Condition: preemption disabled
void sem_wake(sem_t* _Nullable self)
{
    wq_wake(&self->wq, WAKEUP_ALL | WAKEUP_CSW, WRES_WAKEUP);
}
