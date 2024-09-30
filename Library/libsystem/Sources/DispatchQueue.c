//
//  DispatchQueue.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/DispatchQueue.h>
#include <System/_syscall.h>


errno_t DispatchQueue_DispatchSync(int od, Dispatch_Closure _Nonnull func, void* _Nullable context)
{
    return _syscall(SC_dispatch, od, func, context, (unsigned long)__kDispatchOption_Sync);
}

errno_t DispatchQueue_DispatchAsync(int od, Dispatch_Closure _Nonnull func, void* _Nullable context)
{
    return _syscall(SC_dispatch, od, func, context, (unsigned long)0);
}

errno_t DispatchQueue_DispatchAsyncAfter(int od, TimeInterval deadline, Dispatch_Closure _Nonnull func, void* _Nullable context)
{
    return _syscall(SC_dispatch_timer, od, deadline, kTimeInterval_Zero, func, context);
}

errno_t DispatchQueue_DispatchAsyncPeriodically(int od, TimeInterval deadline, TimeInterval interval, Dispatch_Closure _Nonnull func, void* _Nullable context)
{
    return _syscall(SC_dispatch_timer, od, deadline, interval, func, context);
}

int DispatchQueue_GetCurrent(void)
{
    return _syscall(SC_dispatch_queue_current);
}

errno_t DispatchQueue_Create(int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nonnull pOutQueue)
{
    return _syscall(SC_dispatch_queue_create, minConcurrency, maxConcurrency, qos, priority, pOutQueue);
}

errno_t DispatchQueue_Destroy(int od)
{
    return _syscall(SC_dispose, od);
}
