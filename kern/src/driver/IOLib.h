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

#endif /* IOLib_h */
