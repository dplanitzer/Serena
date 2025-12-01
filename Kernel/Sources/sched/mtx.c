//
//  mtx.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "mtx.h"
#include "vcpu.h"
#include <kern/signal.h>

extern errno_t _mtx_unlock(mtx_t* _Nonnull self);
extern errno_t _mtx_unlock_then_wait(mtx_t* _Nonnull self, struct waitqueue* _Nonnull wq);


void mtx_init(mtx_t* _Nonnull self)
{
    self->value = 0;
    wq_init(&self->wq);
    self->owner = NULL;
}

void mtx_deinit(mtx_t* _Nonnull self)
{
    assert(mtx_owner(self) == NULL);
    assert(wq_deinit(&self->wq) == EOK);
}

void mtx_unlock(mtx_t* _Nonnull self)
{
    if (vcpu_current() != self->owner) {
        fatalError(__func__, __LINE__, EPERM);
        /* NOT REACHED */
    }
    
    self->owner = NULL;
    _mtx_unlock(self);
}

errno_t mtx_unlock_then_wait(mtx_t* _Nonnull self, struct waitqueue* _Nonnull wq)
{
    if (vcpu_current() != self->owner) {
        fatalError(__func__, __LINE__, EPERM);
        /* NOT REACHED */
    }
    
    self->owner = NULL;
    return _mtx_unlock_then_wait(self, wq);
}


// Invoked by mtx_lock() if the lock is currently being held by some other VP.
// @Entry Condition: preemption disabled
void mtx_onwait(mtx_t* _Nonnull self)
{
    const errno_t err = wq_wait(&self->wq, &SIGSET_IGNORE_ALL);
    
    if (err != EOK) {
        fatalError(__func__, __LINE__, err);
        /* NOT REACHED */
    }
}

// Invoked by mtx_unlock().
// @Entry Condition: preemption disabled
void mtx_wake(mtx_t* _Nullable self)
{
    wq_wake(&self->wq, WAKEUP_ALL | WAKEUP_CSW, WRES_WAKEUP);
}

// Invoked by mtx_unlock_then_wait().
// @Entry Condition: preemption disabled
errno_t mtx_wake_then_wait(mtx_t* _Nullable self, struct waitqueue* _Nonnull wq)
{
    wq_wake(&self->wq, WAKEUP_ALL, WRES_WAKEUP);
    return wq_wait(wq, NULL);
}
