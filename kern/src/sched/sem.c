//
//  sem.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "sem.h"
#include <assert.h>


void sem_init(sem_t* _Nonnull self, int value)
{
    self->value = value;
    wq_init(&self->wq);
}

void sem_deinit(sem_t* _Nonnull self)
{
    assert(wq_deinit(&self->wq) == EOK);
}

// Invoked by sem_wait() if the semaphore doesn't have the expected
// number of permits.
// @Entry Condition: preemption disabled
void sem_on_wait(sem_t* _Nonnull self)
{
    wq_wait_np(&self->wq, TICKS_MAX);
}

// Invoked by sem_timedwait() if the semaphore doesn't have the expected
// number of permits.
// @Entry Condition: preemption disabled
errno_t sem_on_timedwait(sem_t* _Nonnull self, const nanotime_t* _Nonnull wtp)
{
    return wq_wait_np(&self->wq, wq_calc_deadline(g_mono_clock, TIMER_ABSTIME, wtp));
}

// Invoked by sem_post().
// @Entry Condition: preemption disabled
void sem_wakeup(sem_t* _Nullable self)
{
    wq_wakeup_np(&self->wq, WAKEUP_ONE, 0);
}
