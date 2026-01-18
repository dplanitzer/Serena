//
//  machine/sched.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _MACHINE_SCHED_H
#define _MACHINE_SCHED_H 1

#include <stdnoreturn.h>

//
// CPU Context Switcher API (used by the scheduler)
//

// Saves the current CPU state of the caller and restores the saved CPU state of
// the virtual processor that is scheduled to run. If the sched_state of the
// currently running virtual processor is SCHED_STATE_RUNNING then the state is
// changed to SCHED_STATE_READY and the virtual processor is moved to the correct
// ready queue. Otherwise the sched_state is not changed and the virtual
// processor is not moved to the ready queue because this function assumes that
// the correct state transition has already happend. Eg the vp has been moved to
// waiting or suspended state. 
extern void sched_switch_context(void);

// Restores the state of the boot vp into the currently active CPU context. This
// effectively switches the CPU to the running state.
extern _Noreturn sched_switch_to_boot_vcpu(void);

#endif /* _MACHINE_SCHED_H */
