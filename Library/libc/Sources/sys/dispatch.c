//
//  dispatch.c
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/os_dispatch.h>
#include <kpi/syscall.h>
#include <sys/timespec.h>


int os_dispatch_sync(int od, os_dispatch_func_t _Nonnull func, void* _Nullable context)
{
    return _syscall(SC_disp_schedule, od, func, context, (uint32_t)kDispatchOption_Sync, 0);
}

int os_dispatch_async(int od, os_dispatch_func_t _Nonnull func, void* _Nullable context)
{
    return _syscall(SC_disp_schedule, od, func, context, (uint32_t)0, 0);
}

int os_dispatch_after(int od, const struct timespec* _Nonnull deadline, os_dispatch_func_t _Nonnull func, void* _Nullable context, uintptr_t tag)
{
    return _syscall(SC_disp_timer, od, deadline, &TIMESPEC_ZERO, func, context, tag);
}

int os_dispatch_periodically(int od, const struct timespec* _Nonnull deadline, const struct timespec* _Nonnull interval, os_dispatch_func_t _Nonnull func, void* _Nullable context, uintptr_t tag)
{
    return _syscall(SC_disp_timer, od, deadline, interval, func, context, tag);
}

// Removes all scheduled instances of timers and immediate work items with tag
// 'tag' from the dispatch queue. If the closure of the work item is in the
// process of executing when this function is called then the closure will
// continue to execute uninterrupted. If on the other side, the work item is
// still pending and has not executed yet then it will be removed and it will
// not execute.
int os_dispatch_removebytag(int od, uintptr_t tag)
{
    return _syscall(SC_disp_removebytag, tag);
}

int os_dispatch_getcurrent(void)
{
    return _syscall(SC_disp_getcurrent);
}

int os_dispatch_create(int minConcurrency, int maxConcurrency, int qos, int priority)
{
    int fd;

    if (_syscall(SC_disp_create, minConcurrency, maxConcurrency, qos, priority, &fd) == 0) {
        return fd;
    }
    else {
        return -1;
    }
}

int os_dispatch_destroy(int od)
{
    return _syscall(SC_dispose, od);
}
