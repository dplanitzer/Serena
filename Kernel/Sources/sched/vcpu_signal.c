//
//  vcpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "vcpu.h"
#include "sched.h"
#include <kern/limits.h>
#include <kern/timespec.h>
#include <kpi/signal.h>
#include <log/Log.h>
#include <machine/csw.h>


const sigset_t SIGSET_BLOCK_ALL = UINT32_MAX;
const sigset_t SIGSET_BLOCK_NONE = 0;


// Atomically updates the current signal mask and returns the old mask.
errno_t vcpu_setsigmask(vcpu_t _Nonnull self, int op, sigset_t mask, sigset_t* _Nullable pOutMask)
{
    decl_try_err();
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    const sigset_t oldMask = self->sigmask;

    switch (op) {
        case SIG_SETMASK:
            self->sigmask = mask;
            break;

        case SIG_BLOCK:
            self->sigmask |= mask;
            break;

        case SIG_UNBLOCK:
            self->sigmask &= ~mask;
            break;

        default:
            err = EINVAL;
            break;
    }

    if (err == EOK && pOutMask) {
        *pOutMask = oldMask;
    }

    preempt_restore(sps);
    return err;
}

// @Entry Condition: preemption disabled
errno_t vcpu_sendsignal(vcpu_t _Nonnull self, int signo)
{
    if (signo < SIGMIN || signo > SIGMAX) {
        return EINVAL;
    }


    const sigset_t sigbit = _SIGBIT(signo);

    self->psigs |= sigbit;
    if ((sigbit & ~self->sigmask) == 0) {
        return false;
    }


    if (self->sched_state == SCHED_STATE_WAITING) {
        wq_wakeone(self->waiting_on_wait_queue, self, WAKEUP_CSW, WRES_SIGNAL);
    }
}
