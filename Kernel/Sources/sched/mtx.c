//
//  mtx.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "mtx.h"
#include <dispatcher/VirtualProcessor.h>

extern errno_t _mtx_unlock(mtx_t* _Nonnull self);


void mtx_init(mtx_t* _Nonnull self)
{
    self->value = 0;
    wq_init(&self->wq);
    self->owner_vpid = 0;
}

void mtx_deinit(mtx_t* _Nonnull self)
{
    assert(mtx_ownerid(self) == 0);
    assert(wq_deinit(&self->wq) == EOK);
}

// Unlocks the lock.
errno_t mtx_unlock(mtx_t* _Nonnull self)
{
    const errno_t err = _mtx_unlock(self);
    if (err == EOK) {
        return err;
    }

    fatalError(__func__, __LINE__, err);
}

// Invoked by mtx_lock() if the lock is currently being held by some other VP.
// @Entry Condition: preemption disabled
errno_t mtx_onwait(mtx_t* _Nonnull self)
{
    const errno_t err = wq_wait(&self->wq, &SIGSET_BLOCK_ALL);
    if (err == EOK) {
        return err;
    }

    fatalError(__func__, __LINE__, err);
}

// Invoked by mtx_unlock(). Expects to be called with preemption disabled.
// @Entry Condition: preemption disabled
void mtx_wake(mtx_t* _Nullable self)
{
    wq_wake(&self->wq, WAKEUP_ALL | WAKEUP_CSW, WRES_WAKEUP);
}
