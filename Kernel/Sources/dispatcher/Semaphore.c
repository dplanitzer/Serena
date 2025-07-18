//
//  Semaphore.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Semaphore.h"


// Initializes a new semaphore with 'value' permits
void Semaphore_Init(Semaphore* _Nonnull self, int value)
{
    self->value = value;
    wq_init(&self->wq);
}

// Deinitializes the semaphore.
void Semaphore_Deinit(Semaphore* _Nonnull self)
{
    assert(wq_deinit(&self->wq) == EOK);
}

// Invoked by Semaphore_Acquire() if the semaphore doesn't have the expected
// number of permits.
// @Entry Condition: preemption disabled
errno_t Semaphore_OnWaitForPermits(Semaphore* _Nonnull self, const struct timespec* _Nonnull deadline)
{
    return wq_timedwait(&self->wq,
                        NULL,
                        WAIT_ABSTIME,
                        deadline,
                        NULL);
}

// Invoked by Semaphore_Relinquish().
// @Entry Condition: preemption disabled
void Semaphore_WakeUp(Semaphore* _Nullable self)
{
    wq_wake(&self->wq, WAKEUP_ALL | WAKEUP_CSW, WRES_WAKEUP);
}
