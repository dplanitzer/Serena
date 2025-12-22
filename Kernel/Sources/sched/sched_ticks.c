//
//  sched_quantum.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "sched.h"
#include "vcpu.h"
#include <hal/clock.h>
#include <hal/sched.h>
#include <kern/timespec.h>


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
    if (excpt_frame_isuser(efp) && (run->pending_sigs & SIGSET_URGENTS) != 0) {
        if (cpu_inject_sigurgent(efp)) {
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
        if (run->sched_priority > (SCHED_PRI_LOWEST + 1) && run->qos < SCHED_QOS_REALTIME && run->priority_bias >= SCHED_PRIORITY_BIAS_LOWEST) {
            run->priority_bias--;
            vcpu_sched_params_changed(run);
        }
        sched_set_running(self, rdy);
    }
    else if (rdy->qos > SCHED_QOS_IDLE && run->sched_priority > (SCHED_PRI_LOWEST + 1) && run->qos < SCHED_QOS_REALTIME) {
        run->priority_bias = -(run->effective_priority - rdy->effective_priority);
        vcpu_sched_params_changed(run);
        sched_set_running(self, rdy);
    }
}
