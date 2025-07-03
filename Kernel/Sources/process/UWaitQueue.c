//
//  UWaitQueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "UWaitQueue.h"
#include <dispatcher/VirtualProcessorScheduler.h>
#include <kern/limits.h>
#include <kpi/waitqueue.h>


errno_t UWaitQueue_Create(int policy, UWaitQueueRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    UWaitQueueRef self = NULL;

    switch (policy) {
        case WAITQUEUE_FIFO:
            break;

        default:
            *pOutSelf = NULL;
            return EINVAL;
    }

    err = UResource_AbstractCreate(&kUWaitQueueClass, (UResourceRef*)&self);
    if (err == EOK) {
        WaitQueue_Init(&self->wq);
        self->policy = policy;
    }

    *pOutSelf = self;
    return err;
}

void UWaitQueue_deinit(UWaitQueueRef _Nonnull self)
{
    WaitQueue_Deinit(&self->wq);
}

errno_t UWaitQueue_Wait(UWaitQueueRef _Nonnull self)
{
    decl_try_err();
    const int sps = preempt_disable();

    err = WaitQueue_Wait(&self->wq, WAIT_INTERRUPTABLE);
    preempt_restore(sps);

    return err;
}

errno_t UWaitQueue_SigWait(UWaitQueueRef _Nonnull self, const sigset_t* _Nullable mask, sigset_t* _Nonnull pOutSigs)
{
    decl_try_err();
    const int sps = preempt_disable();

    err = WaitQueue_SigWait(&self->wq, WAIT_INTERRUPTABLE, mask, pOutSigs);
    preempt_restore(sps);

    return err;
}

errno_t UWaitQueue_TimedWait(UWaitQueueRef _Nonnull self, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    const int sps = preempt_disable();
    
    err = WaitQueue_TimedWait(&self->wq, 
                            WAIT_INTERRUPTABLE | flags,
                            wtp,
                            NULL);

    preempt_restore(sps);
    
    return err;
}

errno_t UWaitQueue_SigTimedWait(UWaitQueueRef _Nonnull self, const sigset_t* _Nullable mask, sigset_t* _Nonnull pOutSigs, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    const int sps = preempt_disable();
    
    err = WaitQueue_SigTimedWait(&self->wq,
                            mask, pOutSigs,
                            WAIT_INTERRUPTABLE | flags,
                            wtp);

    preempt_restore(sps);
    
    return err;
}


errno_t UWaitQueue_Wakeup(UWaitQueueRef _Nonnull self, int flags, int signo)
{
    if (signo < 0 || signo > SIGMAX) {
        return EINVAL;
    }

    const int wflags = ((flags & WAKE_ONE) == WAKE_ONE) ? WAKEUP_ONE : WAKEUP_ALL;
    const int sps = preempt_disable();
    
    WaitQueue_Wakeup(&self->wq, wflags | WAKEUP_CSW, signo);
    preempt_restore(sps);
}

class_func_defs(UWaitQueue, UResource,
override_func_def(deinit, UWaitQueue, UResource)
);
