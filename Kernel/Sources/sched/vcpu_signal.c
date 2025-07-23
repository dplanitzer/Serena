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
#include <machine/clock.h>
#include <machine/csw.h>


const sigset_t SIGSET_IGNORE_ALL = 0;


// Atomically updates the current signal mask and returns the old mask.
errno_t vcpu_setsigmask(vcpu_t _Nonnull self, int op, sigset_t mask, sigset_t* _Nullable pOutMask)
{
#if 0
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
#endif
    return ENOTSUP;
}

// @Entry Condition: preemption disabled
errno_t vcpu_sigsend(vcpu_t _Nonnull self, int signo)
{
    if (signo < SIGMIN || signo > SIGMAX) {
        return EINVAL;
    }


    const sigset_t sigbit = _SIGBIT(signo);
    self->pending_sigs |= sigbit;

    if (self->sched_state == SCHED_STATE_WAITING && (self->wait_sigs & sigbit) != 0) {
        wq_wakeone(self->waiting_on_wait_queue, self, WAKEUP_CSW, WRES_SIGNAL);
    }
}

static int _best_pending_sig(vcpu_t _Nonnull self, sigset_t _Nonnull set)
{
    const sigset_t avail_sigs = self->pending_sigs & set;

    if (avail_sigs) {
        for (int i = SIGMIN-1; i < SIGMAX; i++) {
            const sigset_t sigbit = avail_sigs & (1 << i);
            
            if (sigbit) {
                return i + 1;
            }
        }
    }

    return 0;
}

// @Entry Condition: preemption disabled
errno_t vcpu_sigwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int* _Nonnull signo)
{
    vcpu_t vp = (vcpu_t)g_sched->running;

    for (;;) {
        if (wq_prim_wait(wq, set) == WRES_SIGNAL) {
            const int best_signo = _best_pending_sig(vp, *set);

            if (best_signo) {
                vp->pending_sigs &= ~_SIGBIT(best_signo);
                *signo = best_signo;
                return EOK;
            }

            return EINTR;
        }
    }

    /* NOT REACHED */
}

// @Entry Condition: preemption disabled
errno_t vcpu_sigtimedwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nonnull signo)
{
    vcpu_t vp = (vcpu_t)g_sched->running;
    struct timespec now, deadline;
    
    // Convert a relative timeout to an absolute timeout because it makes it
    // easier to deal with spurious wakeups and we won't accumulate math errors
    // caused by time resolution limitations.
    if ((flags & WAIT_ABSTIME) == WAIT_ABSTIME) {
        deadline = *wtp;
    }
    else {
        clock_gettime(g_mono_clock, &now);
        timespec_add(&now, wtp, &deadline);
    }


    for (;;) {
        switch (wq_prim_timedwait(wq, set, flags, &deadline, NULL)) {
            case WRES_WAKEUP:   // Spurious wakeup
                break;

            case WRES_SIGNAL: {
                const int best_signo = _best_pending_sig(vp, *set);

                if (best_signo) {
                    vp->pending_sigs &= ~_SIGBIT(best_signo);
                    *signo = best_signo;
                    return EOK;
                }
                return EINTR;
            }

            case WRES_TIMEOUT:
                return ETIMEDOUT;
        }
    }

    /* NOT REACHED */
}
