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
// SIGSET_CANCELING defines the set of signals that cancel an ongoing system call.
// This set has SIG_FORCE_QUIT and SIG_CANCEL in it. The former is used to terminate
// a process or relinquish just a single vcpu. The later is the canonical signal
// to cancel a system call.
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
// A vcpu uses the vcpu_sigwait() and vcpu_sigtimedwait() call to wait for
// signals. These blocking calls are cancelable by default. This means that they
// will return with an ECANCELED error if one of the signals in the SIGSET_CANCELING
// is pending. This behavior can be turned off by passing the SIGWAIT_NOCANCEL flag.
// Note however that this flag should not be used lightly and only in circumstances
// that absolutely require that a wait ignores canceling signals since this also
// means that sigwait() will ignore a pending SIG_FORCE_QUIT signal.
//
// Note that all canceling and urgent signals are sticky signals (see SIGSET_STICKY).
// They are sticky in the sense that they remain pending for the full duration of
// a system call. They are only consumed once vcpu_syscall_epilog() is invoked at
// the very end of a system call.
//
// The vcpu_syscall_epilog() function handles the canceling and urgent signals.
// The only thing it does in the case of a SIG_CANCEL is that it consumes it.
// It triggers the vcpu relinquish or process termination in the case of a
// SIG_FORCE_QUIT and finally it updates the vcpu state in the case of a
// SIG_URGENT.  

void vcpu_pending_signals(sigset_t* _Nonnull set)
{
    const int sps = preempt_disable();
    vcpu_t vp = g_sched->running;

    *set = vp->pending_sigs;
    preempt_restore(sps);
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

// Expects that 'set' does not contain signals from the SIGSET_STICKY set.
bool _probe_pending_signals(vcpu_t _Nonnull vp, const sigset_t set, int flags, errno_t* _Nonnull err, int* _Nonnull signo)
{
    // Check canceling signals if not disabled. Note that we leave them pending
    // since they will be consumed by the syscall_epilog() function.
    if ((flags & SIGWAIT_NOCANCEL) == 0) {
        if ((vp->pending_sigs & SIGSET_CANCELING) != 0) {
            *err = ECANCELED;
            return true;
        }
    }


    // Check for regular signals.
    const sigset_t hot_sigs = vp->pending_sigs & set;

    if (hot_sigs != 0) {
        *err = EOK;
        *signo = _get_highest_pri_sig(hot_sigs);
        
        vp->pending_sigs &= ~sig_bit(*signo);
        return true;
    }

    return false;
}

#define _probing_sigset(__set) \
((*(__set)) & ~SIGSET_STICKY)

#define _waiting_sigset(__set, __flags) \
((((__flags) & SIGWAIT_NOCANCEL) == 0) ? (*(__set)) | SIGSET_CANCELING : *(__set))

errno_t vcpu_sigwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int flags, int* _Nonnull signo)
{
    decl_try_err();
    const sigset_t probing_sigs = _probing_sigset(set);
    const sigset_t waiting_sigs = _waiting_sigset(set, flags);
    const int sps = preempt_disable();
    vcpu_t vp = (vcpu_t)g_sched->running;

    for (;;) {
        if (_probe_pending_signals(vp, probing_sigs, flags, &err, signo)) {
            break;
        }

        vp->wait_sigs = waiting_sigs;
        wq_wait_np(wq, NULL);
    }
    preempt_restore(sps);

    return err;
}

errno_t vcpu_sigtimedwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int flags, const nanotime_t* _Nonnull wtp, int* _Nonnull signo)
{
    decl_try_err();
    const sigset_t probing_sigs = _probing_sigset(set);
    const sigset_t waiting_sigs = _waiting_sigset(set, flags);
    const ticks_t deadline = wq_calc_deadline(g_mono_clock, flags, wtp);
    
    const int sps = preempt_disable();
    vcpu_t vp = (vcpu_t)g_sched->running;

    for (;;) {
        if (_probe_pending_signals(vp, probing_sigs, flags, &err, signo)) {
            break;
        }

        vp->wait_sigs = waiting_sigs;
        err = wq_wait_np(wq, &deadline);
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

    if ((sigs & sig_bit(SIG_FORCE_QUIT)) != 0) {
        if (self->proc->terminator_vcpu) {
            Process_Terminate(self->proc, WAIT_REASON_SIGNALED, self->proc->signo_causing_termination);
        }
        else {
            Process_RelinquishVirtualProcessor(self->proc, self);
        }

        /* NOT REACHED */
    }


    if ((sigs & sig_bit(SIG_URGENT)) != 0) {
        const int sps = preempt_disable();
        _vcpu_do_deferred_suspend_np(self);
        preempt_restore(sps);
    }


    self->pending_sigs &= ~SIGSET_STICKY;
}
