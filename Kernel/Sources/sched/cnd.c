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
#include <machine/sched.h>


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
    
    // Don't do a WAKEUP_CSW here because we are currently holding the mutex and
    // thus the other guy would not be able to grab the mutex and all we would
    // end up doing is a useless CSW from us to the other guy and the other guy
    // then has to CSW back to us when it tries to take the mutex that we are
    // still holding. 
    wq_wake(&self->wq, flags, WRES_WAKEUP);
    preempt_restore(sps);
}

// Unlocks 'pLock' and blocks the caller until the condition variable is signaled.
// It then locks 'pLock' before it returns to the caller.
errno_t cnd_wait(cnd_t* _Nonnull self, mtx_t* _Nonnull mtx)
{
    // Note that we have to unlock the mutex and enter the wait state atomically.
    // This is crucially important to ensure that we can not miss a wakeup.
    // Assuming that unlocking the mutex and entering the wait wouldn't be
    // atomic, then a wakeup miss could happen like this:
    // P -> producer
    // C -> consumer
    // - consumer holds the mutex
    // - consumer unlocks the mutex
    // -- producer grabs the mutex
    // -- producer does a broadcast()
    // -- producer drops the mutex
    // - consumer enters the wait state
    // --> consumer is now stuck because it missed the broadcast() since it no
    // longer held the mutex but hadn't entered the wait state yet. The producer
    // was able to sneak in between and do a broadcast() before the consumer was
    // ready to receive it.
    const errno_t err = mtx_unlock_then_wait(mtx, &self->wq);
    mtx_lock(mtx);

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
