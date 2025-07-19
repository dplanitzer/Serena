//
//  cnd.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "cnd.h"
#include "sched.h"
#include <kern/assert.h>


// Initializes a new condition variable.
void cnd_init(cnd_t* _Nonnull self)
{
    wq_init(&self->wq);
}

// Deinitializes the condition variable.
void cnd_deinit(cnd_t* _Nonnull self)
{
    assert(wq_deinit(&self->wq) == EOK);
}

// Signals the given condition variable.
void _cnd_wake(cnd_t* _Nonnull self, bool broadcast)
{
    const int flags = (broadcast) ? WAKEUP_ALL : WAKEUP_ONE;
    const int sps = preempt_disable();
    
    wq_wake(&self->wq, flags | WAKEUP_CSW, WRES_WAKEUP);
    preempt_restore(sps);
}

// Unlocks 'pLock' and blocks the caller until the condition variable is signaled.
// It then locks 'pLock' before it returns to the caller.
errno_t cnd_wait(cnd_t* _Nonnull self, mtx_t* _Nonnull mtx)
{
    // Note that we disable preemption while unlocking and entering the wait.
    // The reason is that we want to ensure that noone else can grab the lock,
    // do a signal and then unlock between us unlocking and trying to enter the
    // wait. If we would allow this, then we would miss a wakeup. An alternative
    // strategy to this one here would be to use a stateful wait (aka signalling
    // wait).
    const int sps = preempt_disable();
    
    mtx_unlock(mtx);
    const int err = wq_wait(&self->wq, NULL);
    mtx_lock(mtx);

    preempt_restore(sps);
    
    return err;
}

// Unlocks 'pLock' and blocks the caller until the condition variable is signaled.
// It then locks 'pLock' before it returns to the caller.
errno_t cnd_timedwait(cnd_t* _Nonnull self, mtx_t* _Nonnull mtx, const struct timespec* _Nonnull deadline)
{
    const int sps = preempt_disable();
    
    mtx_unlock(mtx);
    const int err = wq_timedwait(&self->wq,
                                NULL,
                                WAIT_ABSTIME,
                                deadline,
                                NULL);
    mtx_lock(mtx);

    preempt_restore(sps);
    
    return err;
}
