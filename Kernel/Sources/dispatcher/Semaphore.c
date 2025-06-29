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
    WaitQueue_Init(&self->wq);
}

// Deinitializes the semaphore.
void Semaphore_Deinit(Semaphore* _Nonnull self)
{
    assert(WaitQueue_Deinit(&self->wq) == EOK);
}

// Invoked by Semaphore_Acquire() if the semaphore doesn't have the expected
// number of permits.
// @Entry Condition: preemption disabled
errno_t Semaphore_OnWaitForPermits(Semaphore* _Nonnull self, const struct timespec* _Nonnull deadline)
{
    return WaitQueue_TimedWait(&self->wq,
                        WAIT_INTERRUPTABLE | WAIT_ABSTIME,
                        deadline,
                        NULL);
}

// Invoked by Semaphore_Relinquish().
// @Entry Condition: preemption disabled
void Semaphore_WakeUp(Semaphore* _Nullable self)
{
    WaitQueue_Wakeup(&self->wq, WAKEUP_ALL | WAKEUP_CSW, WAKEUP_REASON_FINISHED);
}
