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
#include <kern/types.h>

//
// CPU Context Switcher API (used by the scheduler)
//

extern void sched_switch_context(void);
extern _Noreturn sched_switch_to_boot_vcpu(void);

#endif /* _MACHINE_SCHED_H */
