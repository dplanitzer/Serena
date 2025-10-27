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

    if (upc >= (uintptr_t)sigurgent && upc < (uintptr_t)sigurgent_end) {
        return false;
    }

    usp_set(sp_push_rts(usp_get(), (void*)excpt_frame_getpc(efp)));
    excpt_frame_setpc(efp, sigurgent);
    
    return true;
}


// Invoked by the clock when a wait timeout expires
void sched_wait_timeout_irq(vcpu_t _Nonnull vp)
{
    wq_wakeone(vp->waiting_on_wait_queue, vp, 0, WRES_TIMEOUT);
}


// Invoked at the end of every clock tick.
void sched_tick_irq(sched_t _Nonnull self, excpt_frame_t* _Nonnull efp)
{
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
    run->quantum_countdown--;
    if (run->quantum_countdown > 0) {
        return;
    }

    
    // The time slice has expired. Check whether there's another VP on the ready
    // queue which is more important. If so we context switch to that guy.
    // Otherwise we'll continue to run for another time slice.
    register vcpu_t rdy = sched_highest_priority_ready(self);
    if (rdy == NULL) {
        return;
    }

    if (rdy->effective_priority >= run->effective_priority) {
        if (run->qos > SCHED_QOS_IDLE && run->qos < SCHED_QOS_REALTIME && run->priority_bias >= SCHED_PRIORITY_BIAS_LOWEST) {
            run->priority_bias--;
            vcpu_sched_params_changed(run);
        }
        sched_set_running(self, rdy, true);
    }
    else if (rdy->qos > SCHED_QOS_IDLE && run->qos < SCHED_QOS_REALTIME && (run->qos > SCHED_QOS_BACKGROUND || (run->qos == SCHED_QOS_BACKGROUND && run->qos_priority > QOS_PRI_LOWEST))) {
        run->priority_bias = -(run->effective_priority - rdy->effective_priority);
        vcpu_sched_params_changed(run);
        sched_set_running(self, rdy, true);
    }
}
