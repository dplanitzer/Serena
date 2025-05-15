//
//  dispatch.c
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/dispatch.h>
#include <sys/_syscall.h>
#include <sys/timespec.h>


errno_t dispatch_sync(int od, dispatch_func_t _Nonnull func, void* _Nullable context)
{
    return _syscall(SC_disp_schedule, od, func, context, (uint32_t)kDispatchOption_Sync, 0);
}

errno_t dispatch_async(int od, dispatch_func_t _Nonnull func, void* _Nullable context)
{
    return _syscall(SC_disp_schedule, od, func, context, (uint32_t)0, 0);
}

errno_t dispatch_after(int od, struct timespec deadline, dispatch_func_t _Nonnull func, void* _Nullable context, uintptr_t tag)
{
    return _syscall(SC_disp_timer, od, deadline, TIMESPEC_ZERO, func, context, tag);
}

errno_t dispatch_periodically(int od, struct timespec deadline, struct timespec interval, dispatch_func_t _Nonnull func, void* _Nullable context, uintptr_t tag)
{
    return _syscall(SC_disp_timer, od, deadline, interval, func, context, tag);
}

// Removes all scheduled instances of timers and immediate work items with tag
// 'tag' from the dispatch queue. If the closure of the work item is in the
// process of executing when this function is called then the closure will
// continue to execute uninterrupted. If on the other side, the work item is
// still pending and has not executed yet then it will be removed and it will
// not execute.
errno_t dispatch_removebytag(int od, uintptr_t tag)
{
    return _syscall(SC_disp_removebytag, tag);
}

int dispatch_getcurrent(void)
{
    return _syscall(SC_disp_getcurrent);
}

errno_t dispatch_create(int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nonnull pOutQueue)
{
    return _syscall(SC_disp_create, minConcurrency, maxConcurrency, qos, priority, pOutQueue);
}

errno_t dispatch_destroy(int od)
{
    return _syscall(SC_dispose, od);
}
