//
//  kerneld.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERNELD_H_
#define _KERNELD_H_

#include <process/Process.h>


extern ProcessRef _Nonnull  gKernelProcess;

#define PID_KERNEL  1


// Initializes the kerneld process and adopts the calling vcpu as kerneld's main
// vcpu.
extern errno_t kerneld_init(void);

// Spawns systemd from the kernel process context.
extern errno_t kerneld_spawn_systemd(ProcessRef _Nonnull self, FileHierarchyRef _Nonnull fh);

#endif /* _KERNELD_H_ */
