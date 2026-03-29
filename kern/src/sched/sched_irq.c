//
//  sched_irq.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "sched.h"
#include "vcpu.h"
#include <ext/timespec.h>
#include <hal/clock.h>
#include <hal/sched.h>


// Invoked by the clock when a wait timeout expires
void sched_wait_timeout_irq(vcpu_t _Nonnull vp)
{
    wq_wakeup_vcpu(vp->waiting_on_wait_queue, vp, 0, WRES_TIMEOUT, 0);
}


// Invoked at every clock tick and before sched_on_any_irq() is invoked. Runs in
// the interrupt context.
void sched_on_tick_irq(sched_t _Nonnull self, excpt_frame_t* _Nonnull efp)
{
    register vcpu_t run = (vcpu_t)self->running;

    if (run->quantum_countdown > 0) {
        run->quantum_countdown -= SCHED_QUANTUM_SCALE;
    }

    if (excpt_frame_isuser(efp)) {
        run->user_ticks++;
    }
    else {
        run->system_ticks++;
    }
}


// Invoked at the end of any and all interrupts. Runs in the interrupt context.
void sched_on_any_irq(sched_t _Nonnull self, excpt_frame_t* _Nonnull efp)
{
    register vcpu_t run = (vcpu_t)self->running;

    // Redirect the currently running VP to sigurgent() if it is running in user
    // mode, has an urgent signal pending and we haven't already triggered a
    // redirect previously.
    if (excpt_frame_isuser(efp) && (run->pending_sigs & SIGSET_URGENTS) != 0) {
        cpu_inject_sigurgent(efp);
    }


    if (run->quantum_countdown > 0) {
        // Quantum isn't done yet. Check for preemption
        register vcpu_t rdy = sched_highest_priority_ready(self);

        if (rdy && rdy->cur_priority > run->cur_priority) {
            sched_set_running_as_preemptor(self, rdy);
        }
    }
    else {
        // Quantum finished. Figure out who should run next or whether we should
        // run another quantum
        register vcpu_t rdy = sched_highest_priority_ready(self);

        if (rdy && rdy->cur_priority >= run->cur_priority) {
            sched_set_running(self, rdy);
        }


        // Is this a dynamic priority vcpu?
        if (!vcpu_is_fixed_pri(run)) {
            bool do_sched_params_changed = false;
            
            // Apply a penalty to the vcpu if it has never blocked
            if ((run->flags & VP_FLAG_DID_WAIT) == 0) {
                run->priority_boost = 0;
                run->quantum_boost = 0;
                
                if (run->priority_penalty < SCHED_PRI_HIGHEST) {
                    run->priority_penalty++;
                }
                do_sched_params_changed = true;
            }


            // Decay any boost value that may exist
            if (run->priority_boost > 0) {
                run->priority_boost--;
                do_sched_params_changed = true;
            }


            if (do_sched_params_changed) {
                vcpu_on_sched_param_changed(run);
            }
        }

        run->flags &= ~VP_FLAG_DID_WAIT;


        // Reset the quantum
        vcpu_reset_quantum(run);
    }
}