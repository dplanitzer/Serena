//
//  mtx.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "mtx.h"
#include "vcpu.h"
#include <assert.h>
#include <kern/kernlib.h>
#include <kern/sigset.h>

extern errno_t _mtx_unlock(mtx_t* _Nonnull self);
extern errno_t _mtx_unlock_then_wait(mtx_t* _Nonnull self, struct waitqueue* _Nonnull wq, ticks_t deadline);


void mtx_init(mtx_t* _Nonnull self)
{
    *self = MTX_INIT;
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

errno_t mtx_unlock_then_wait(mtx_t* _Nonnull self, struct waitqueue* _Nonnull wq, ticks_t deadline)
{
    if (vcpu_current() != self->owner) {
        fatalError(__func__, __LINE__, EPERM);
        /* NOT REACHED */
    }
    
    self->owner = NULL;
    return _mtx_unlock_then_wait(self, wq, deadline);
}


// Invoked by mtx_lock() if the lock is currently being held by some other VP.
// @Entry Condition: preemption disabled
void mtx_onwait(mtx_t* _Nonnull self)
{
    wq_wait_np(&self->wq, TICKS_MAX);
}

// Invoked by mtx_unlock().
// @Entry Condition: preemption disabled
void mtx_wake(mtx_t* _Nullable self)
{
    wq_wakeup_np(&self->wq, WAKEUP_ALL, 0);
}

// Invoked by mtx_unlock_then_wait().
// @Entry Condition: preemption disabled
errno_t mtx_wake_then_wait(mtx_t* _Nullable self, struct waitqueue* _Nonnull wq, ticks_t deadline)
{
    wq_wakeup_np(&self->wq, WAKEUP_ALL | WAKEUP_NO_IMMED_CSW, 0);
    wq_wait_np(wq, deadline);

    return EOK;
}
