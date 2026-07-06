//
//  IOLib.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/4/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef IOLib_h
#define IOLib_h

#include <ext/try.h>
#include <kpi/_time.h>
#include <kpi/vcpu.h>

extern errno_t IOInit(void);

// Blocks the caller for at least 'us' microseconds. The wait is implemented as
// a cpu spin operation. Consequently only use this function for very short waits
// where it is important that only a small amount of overhead is added to the
// specified waiting time. 
extern void IODelay(useconds_t us);

// Blocks the caller for at least 'ms' milliseconds. The wait is implemented as
// a true idle wait. Consequently the wait may be up to a few milliseconds longer
// than what was specified. This is the preferred way to wait.
extern void IOSleep(mseconds_t ms);


// Acquires a virtual processor for IO work. 'qos' is the quality of service
// level, 'priority' is the priority, 'func' the function that will be invoked
// on the acquired vcpu and 'arg' is the argument that will be passed to the
// new vcpu. The virtual processor will be relinquished when 'func' returns.
extern errno_t IOAcquireVirtualProcessor(vcpu_func_t _Nonnull func, void* _Nullable arg, int qos, int priority, vcpu_t _Nullable * _Nonnull pOutVp);

// Resume the I/O virtual processor 'vcpu'. Call this function on a newly acquired
// virtual processor after you've finished initializing teh data on which the
// vcpu will depend.
extern void IOResumeVirtualProcessor(vcpu_t _Nonnull vcpu);

#endif /* IOLib_h */
