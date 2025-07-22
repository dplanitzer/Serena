//
//  sched_quantum.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "sched.h"
#include "vcpu.h"
#include <machine/clock.h>
#include <machine/csw.h>
#include <kern/timespec.h>

extern void sigurgent(void);
extern void sigurgent_end(void);


// Injects a call to sigurgent(). This is done if we detect that teh currently
// running vcpu is running in user space and has a signal pending that requires
// urgent delivery. The sigurgent() system call itself does nothing but it gives
// the system call handler a chance to look at the pending signal and handle it
// as required.
// Note that we set up the sigurgent() injection in such a way that it can return
// back to user space and that the vcpu will be able to continue with whatever
// it was doing before the injection. We do this by pushing a RTS frame on the
// vcpu's user stack that will guide the vcpu back to the original point of
// interruption.
// Also note that we ensure that we do not try to inject a sigurgent while the
// vcpu is still executing inside an earlier sigurgent injection.
static bool inject_sigurgent_call(excpt_frame_t* _Nonnull efp)
{
    const uintptr_t upc = excpt_frame_getpc(efp);

    if (upc < (uintptr_t)sigurgent || upc > (uintptr_t)sigurgent_end) {
        cpu_push_user_rts(excpt_frame_getpc(efp));
        
        excpt_frame_setpc(efp, sigurgent);
        return true;
    }
    else {
        return false;
    }
}


// Invoked at the end of every quantum.
void sched_quantum_irq(excpt_frame_t* _Nonnull efp)
{
    register sched_t self = g_sched;

    // First, go through the timeout queue and move all VPs whose timeouts have
    // expired to the ready queue.
    const Quantums now = clock_getticks(g_mono_clock);
    
    while (self->timeout_queue.first) {
        register sched_timeout_t* ct = (sched_timeout_t*)self->timeout_queue.first;
        
        if (ct->deadline > now) {
            break;
        }
        
        vcpu_t vp = VP_FROM_TIMEOUT(ct);
        wq_wakeone(vp->waiting_on_wait_queue, vp, 0, WRES_TIMEOUT);
    }
    

    register vcpu_t run = (vcpu_t)self->running;

    // Redirect the currently running VP to sigurgent() if it is running in user
    // mode, has an urgent signal pending and we haven't already triggered a
    // redirect previously.
    if (excpt_frame_isuser(efp) && (run->pending_sigs & SIGSET_URGENTS)) {
        if (inject_sigurgent_call(efp)) {
            return;
        }
    }


    // Update the time slice info for the currently running VP
    run->quantum_allowance--;
    if (run->quantum_allowance > 0) {
        return;
    }

    
    // The time slice has expired. Lower our priority and then check whether
    // there's another VP on the ready queue which is more important. If so we
    // context switch to that guy. Otherwise we'll continue to run for another
    // time slice.
    run->effectivePriority = __max(run->effectivePriority - 1, VP_PRIORITY_LOWEST);
    run->quantum_allowance = QuantumAllowanceForPriority(run->effectivePriority);

    register vcpu_t rdy = sched_highest_priority_ready(self);
    if (rdy == NULL || rdy->effectivePriority <= run->effectivePriority) {
        // We didn't find anything better to run. Continue running the currently
        // running VP.
        return;
    }
    
    
    // Move the currently running VP back to the ready queue and pull the new
    // VP off the ready queue
    sched_remove_vcpu_locked(self, rdy);
    sched_add_vcpu_locked(self, run, run->priority);

    
    // Request a context switch
    self->scheduled = rdy;
    self->csw_signals |= CSW_SIGNAL_SWITCH;
}
