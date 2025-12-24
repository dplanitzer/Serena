//
//  vcpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "vcpu.h"
#include "sched.h"
#include <limits.h>
#include <hal/clock.h>
#include <hal/sched.h>
#include <kern/timespec.h>
#include <kpi/signal.h>


static errno_t _vcpu_sigsend(vcpu_t _Nonnull self, int flags, int signo)
{
    decl_try_err();

    if (signo < SIGMIN || signo > SIGMAX) {
        return EINVAL;
    }

    const sigset_t sigbit = _SIGBIT(signo);
    const int sps = preempt_disable();
    self->pending_sigs |= sigbit;

    if (signo == SIGKILL || signo == SIGVPRQ) {
        // Do a force resume to ensure that the guy picks up the termination
        // request right away.
        vcpu_resume(self, true);
    }
        
    if ((self->wait_sigs & sigbit) != 0) {
        wq_wakeone(self->waiting_on_wait_queue, self, flags, WRES_SIGNAL);
    }
    preempt_restore(sps);

    return err;
}

errno_t vcpu_sigsend(vcpu_t _Nonnull self, int signo)
{
    return _vcpu_sigsend(self, WAKEUP_CSW, signo);
}

errno_t vcpu_sigsend_irq(vcpu_t _Nonnull self, int signo)
{
    //XXX enabling this breaks the proc_exit and vcpu_sched tests (they get stuck)
    return _vcpu_sigsend(self, /*WAKEUP_CSW | WAKEUP_IRQ*/ 0, signo);
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

errno_t vcpu_sigwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int* _Nonnull signo)
{
    const int sps = preempt_disable();
    vcpu_t vp = (vcpu_t)g_sched->running;
    bool done = false;
    errno_t err;

    while (!done) {
        if (wq_prim_wait(wq, set, false) == WRES_SIGNAL) {
            const int best_signo = _consume_best_pending_sig(vp, *set);

            if (best_signo) {
                *signo = best_signo;
                err = EOK;
            }
            else {
                err = EINTR;
            }

            done = true;
        }
    }

    preempt_restore(sps);
    return err;
}

errno_t vcpu_sigtimedwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nonnull signo)
{
    const int sps = preempt_disable();
    vcpu_t vp = (vcpu_t)g_sched->running;
    struct timespec now, deadline;
    bool done = false;
    errno_t err;
    
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


    while (!done) {
        switch (wq_prim_timedwait(wq, set, flags, &deadline, NULL)) {
            case WRES_WAKEUP:   // Spurious wakeup
                break;

            case WRES_SIGNAL: {
                const int best_signo = _consume_best_pending_sig(vp, *set);

                if (best_signo) {
                    *signo = best_signo;
                    err = EOK;
                }
                else {
                    err = EINTR;
                }

                done = true;
                break;
            }

            case WRES_TIMEOUT:
                err = ETIMEDOUT;
                done = true;
                break;
        }
    }

    preempt_restore(sps);
    return err;
}
