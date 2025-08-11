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


// Atomically updates the routing information for process-targeted signals.
errno_t vcpu_sigroute(vcpu_t _Nonnull self, int op)
{
    decl_try_err();
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();

    switch (op) {
        case SIG_ROUTE_DISABLE:
            if (self->proc_sigs_enabled > 0) {
                self->proc_sigs_enabled--;
            }
            else {
                err = EOVERFLOW;
            }
            break;

        case SIG_ROUTE_ENABLE:
            if (self->proc_sigs_enabled < INT_MAX) {
                self->proc_sigs_enabled++;
            }
            else {
                err = EOVERFLOW;
            }
            break;

        default:
            err = EINVAL;
            break;
    }
    preempt_restore(sps);

    return err;
}

// Forcefully turn process-targeted signal routing off for the given VP.
void vcpu_sigrouteoff(vcpu_t _Nonnull self)
{
    const int sps = preempt_disable();

    self->proc_sigs_enabled = 0;
    preempt_restore(sps);
}

static errno_t _vcpu_sigsend(vcpu_t _Nonnull self, int flags, int signo, bool isProc)
{
    decl_try_err();

    if (signo < SIGMIN || signo > SIGMAX) {
        return EINVAL;
    }

    const int sps = preempt_disable();
    if (!isProc || (isProc && self->proc_sigs_enabled > 0)) {
        const sigset_t sigbit = _SIGBIT(signo);
        
        self->pending_sigs |= sigbit;

        if (self->sched_state == SCHED_STATE_WAITING && (self->wait_sigs & sigbit) != 0) {
            wq_wakeone(self->waiting_on_wait_queue, self, flags, WRES_SIGNAL);
        }
    }
    preempt_restore(sps);

    return err;
}

errno_t vcpu_sigsend(vcpu_t _Nonnull self, int signo, bool isProc)
{
    return _vcpu_sigsend(self, WAKEUP_CSW, signo, isProc);
}

errno_t vcpu_sigsend_irq(vcpu_t _Nonnull self, int signo, bool isProc)
{
    return _vcpu_sigsend(self, 0, signo, isProc);
}

sigset_t vcpu_sigpending(vcpu_t _Nonnull self)
{
    const int sps = preempt_disable();
    const sigset_t set = self->pending_sigs;
    preempt_restore(sps);

    return set;
}

bool vcpu_aborting(vcpu_t _Nonnull self)
{
    const int sps = preempt_disable();
    const bool r = ((self->pending_sigs & _SIGBIT(SIGKILL)) != 0) ? true : false;
    preempt_restore(sps);
    return r;
}

static int _consume_best_pending_sig(vcpu_t _Nonnull self, sigset_t _Nonnull set)
{
    const sigset_t avail_sigs = self->pending_sigs & set;

    if (avail_sigs) {
        for (int i = SIGMIN-1; i < SIGMAX; i++) {
            const sigset_t sigbit = avail_sigs & (1 << i);
            
            if (sigbit) {
                const int signo = i + 1;

                if (signo != SIGKILL) {
                    self->pending_sigs &= ~sigbit;
                }
                return signo;
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
            const int best_signo = _consume_best_pending_sig(vp, *set);

            if (best_signo) {
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
                const int best_signo = _consume_best_pending_sig(vp, *set);

                if (best_signo) {
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
