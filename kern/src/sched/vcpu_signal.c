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
#include <kern/sigset.h>
#include <process/ProcessPriv.h>


// Signals, canceling, urgents and system calls
//
// SIGSET_URGENTS defines the set of signals that require urgent attention from
// a vcpu. Note that this signal interrupts a vcpu if it is executing in user
// space and the signal triggers a call to the sig_urgent() system call. This
// call is injected by the context switcher. The sig_urgent() system call itself
// does nothing interesting but it creates an opportunity for the system call
// handler to invoke vcpu_syscall_epilog().
//
// Note that SIG_URGENT does not cancel or otherwise interrupt an ongoing system
// call. It is only handled at the very end of a system call and it is ignored
// and left pending while the system call is doing its work.
//
// A vcpu uses the vcpu_sigwait() call to wait for signals. These blocking calls
// are cancelable by default. This means that they will return with an ECANCELED
// error if one of the signals in the SIGSET_CANCELING is pending. This behavior
// can be turned off by passing the SIGWAIT_NOABORT flag. Note however that
// this flag should not be used lightly and only in circumstances that
// absolutely require that a wait ignores canceling signals since this also
// means that sigwait() will ignore a pending SIG_FORCE_QUIT signal.
//
// Note that all canceling and urgent signals are sticky signals (see SIGSET_STICKY).
// They are sticky in the sense that they remain pending for the full duration of
// a system call. They are only consumed once vcpu_syscall_epilog() is invoked at
// the very end of a system call.
//
// The vcpu_syscall_epilog() function handles vcpu aborts and urgent signals. A
// system call is aborted, the vcpu relinquished or the whole process terminated
// if a SIG_FORCE_QUIT is pending. A pending SIG_URGENT causes the vcpu to
// inspect some of its kernel state (i.e. suspend related state) and update its
// overall state accordingly.

void vcpu_pending_signals(sigset_t* _Nonnull set)
{
    const int sps = preempt_disable();
    vcpu_t vp = g_sched->running;

    *set = vp->pending_sigs;
    preempt_restore(sps);
}

errno_t vcpu_testabort_np(void)
{
    return ((g_sched->running->pending_sigs & SIGSET_ABORTS) != 0) ? EABORTED : EOK;
}


errno_t vcpu_send_signal_boost(vcpu_t _Nonnull self, int signo, int pri_boost)
{
    decl_try_err();

    if (signo < SIG_MIN || signo > SIG_MAX) {
        return EINVAL;
    }

    const sigset_t sigbit = sig_bit(signo);
    const int sps = preempt_disable();
    self->pending_sigs |= sigbit;

    if (signo == SIG_FORCE_QUIT) {
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


// NOTE: assumes that 'set' has at least one signal set
static int _get_highest_pri_sig(sigset_t set)
{
    int signo = 0;

    for (int i = SIG_MIN-1; i < SIG_MAX; i++) {
        const sigset_t sigbit = set & (1 << i);
            
        if (sigbit) {
            signo = i + 1;
            break;
        }
    }

    return signo;
}

errno_t vcpu_sigwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int flags, const ticks_t deadline, int* _Nonnull signo)
{
    decl_try_err();
    sigset_t awaiting_sigs, consumable_sigs;
    const bool doAbort = (flags & SIGWAIT_NOABORT) == 0;


    consumable_sigs = *set;
    consumable_sigs &= ~SIGSET_ABORTS;
    consumable_sigs &= ~SIGSET_URGENTS;

    awaiting_sigs = consumable_sigs;
    if (doAbort) {
        awaiting_sigs |= SIGSET_ABORTS;
    }


    const int sps = preempt_disable();
    vcpu_t vp = (vcpu_t)g_sched->running;

    for (;;) {
        if ((vp->pending_sigs & awaiting_sigs) != 0) {
            // Abort has priority
            if (doAbort && vcpu_testabort_np() == EABORTED) {
                err = EABORTED;
                break;
            }

            // Check for signals in 'set' next
            const int hp_sig = _get_highest_pri_sig(vp->pending_sigs & consumable_sigs);
        
            vp->pending_sigs &= ~sig_bit(hp_sig);
            *signo = hp_sig;
            break;
        }


        vp->wait_sigs = awaiting_sigs;
        err = wq_wait_np(wq, deadline);
        if (err != EOK) {
            break;
        }
    }
    preempt_restore(sps);
    
    return err;
}


void vcpu_syscall_epilog(vcpu_t _Nonnull self)
{
    //XXX should do the pending signal check and consumption of the sticky bits
    //XXX atomically. For that matter, all pending signal stuff should be done
    //XXX with an atomic sigset so that we don't have to turn off preemption
    //XXX just to do simple sigset stuff.
    const sigset_t sigs = self->pending_sigs;

    if ((sigs & SIGSET_ABORTS) != 0) {
        if (self->proc->terminator_vcpu) {
            Process_Terminate(self->proc, WAIT_REASON_SIGNALED, self->proc->signo_causing_termination);
        }
        else {
            Process_RelinquishCurrentVirtualProcessor(self->proc);
        }

        /* NOT REACHED */
    }


    if ((sigs & SIGSET_URGENTS) != 0) {
        const int sps = preempt_disable();
        _vcpu_do_deferred_suspend_np(self);
        preempt_restore(sps);
    }


    self->pending_sigs &= ~SIGSET_STICKY;
}
