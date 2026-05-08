//
//  vcpu_signal.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "vcpu.h"
#include "sched.h"
#include <limits.h>
#include <ext/nanotime.h>
#include <hal/clock.h>
#include <hal/sched.h>
#include <kpi/signal.h>


errno_t vcpu_send_signal_boost(vcpu_t _Nonnull self, int signo, int pri_boost)
{
    decl_try_err();

    if (signo < SIG_MIN || signo > SIG_MAX) {
        return EINVAL;
    }

    const sigset_t sigbit = sig_bit(signo);
    const int sps = preempt_disable();
    self->pending_sigs |= sigbit;

    if (signo == SIG_TERMINATE) {
        // Do a force resume to ensure that the guy picks up the termination
        // request right away.
        vcpu_resume(self, true);
    }
        
    if ((self->wait_sigs & sigbit) != 0) {
        wq_wakeup_vcpu_np(self->waiting_on_wait_queue, self, 0, pri_boost);
    }
    preempt_restore(sps);

    return err;
}

sigset_t vcpu_pending_signals(vcpu_t _Nonnull self)
{
    const int sps = preempt_disable();
    const sigset_t set = self->pending_sigs;
    preempt_restore(sps);

    return set;
}

bool vcpu_is_aborting(vcpu_t _Nonnull self)
{
    const int sps = preempt_disable();
    const bool r = ((self->pending_sigs & sig_bit(SIG_TERMINATE)) != 0) ? true : false;
    preempt_restore(sps);
    return r;
}

static int _consume_best_pending_sig2(vcpu_t _Nonnull self, sigset_t _Nonnull set)
{
    const sigset_t avail_sigs = self->pending_sigs & set;

    if (avail_sigs) {
        for (int i = SIG_MIN-1; i < SIG_MAX; i++) {
            const sigset_t sigbit = avail_sigs & (1 << i);
            
            if (sigbit) {
                const int signo = i + 1;

                if (signo != SIG_TERMINATE) {
                    self->pending_sigs &= ~sigbit;
                }
                return signo;
            }
        }
    }

    return 0;
}

// NOTE: assumes that 'hot_sigs' has at least one signal set
static int _consume_best_pending_sig(vcpu_t _Nonnull self, sigset_t hot_sigs)
{
    int signo = 0;

    for (int i = SIG_MIN-1; i < SIG_MAX; i++) {
        const sigset_t sigbit = hot_sigs & (1 << i);
            
        if (sigbit) {
            signo = i + 1;
            break;
        }
    }

    if (signo != SIG_TERMINATE) {
        self->pending_sigs &= ~sig_bit(signo);
    }
    return signo;
}

void vcpu_sigwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int* _Nonnull signo)
{
    const int sps = preempt_disable();
    vcpu_t vp = (vcpu_t)g_sched->running;

    for (;;) {
        const sigset_t hot_sigs = vp->pending_sigs & (*set);

        if (hot_sigs != 0) {
            *signo = _consume_best_pending_sig(vp, hot_sigs);
            break;
        }

        vp->wait_sigs = *set;
        wq_wait_np(wq);
    }
    preempt_restore(sps);
}

errno_t vcpu_sigtimedwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int flags, const nanotime_t* _Nonnull wtp, int* _Nonnull signo)
{
    decl_try_err();
    nanotime_t now, deadline;
    
    // Convert a relative timeout to an absolute timeout because it makes it
    // easier to deal with spurious wakeups and we won't accumulate math errors
    // caused by time resolution limitations.
    if ((flags & WAIT_ABSTIME) == WAIT_ABSTIME) {
        deadline = *wtp;
    }
    else {
        clock_gettime(g_mono_clock, &now);
        nanotime_add(&now, wtp, &deadline);
        flags |= WAIT_ABSTIME;
    }


    const int sps = preempt_disable();
    vcpu_t vp = (vcpu_t)g_sched->running;

    for (;;) {
        const sigset_t hot_sigs = vp->pending_sigs & (*set);

        if (hot_sigs != 0) {
            *signo = _consume_best_pending_sig(vp, hot_sigs);
            break;
        }

        vp->wait_sigs = *set;
        if (wq_timedwait_np(wq, flags, &deadline)) {
            err = ETIMEDOUT;
            break;
        }
    }
    preempt_restore(sps);
    
    return err;
}
