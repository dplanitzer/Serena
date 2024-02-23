//
//  DispatchQueue.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/DispatchQueue.h>
#include <System/_syscall.h>


errno_t DispatchQueue_DispatchSync(int od, Dispatch_Closure _Nonnull pClosure, void* _Nullable pContext)
{
    return _syscall(SC_dispatch, od, (unsigned long)kDispatchOption_Sync, pClosure, pContext);
}

errno_t DispatchQueue_DispatchAsync(int od, Dispatch_Closure _Nonnull pClosure, void* _Nullable pContext)
{
    return _syscall(SC_dispatch, od, (unsigned long)0, pClosure, pContext);
}

errno_t DispatchQueue_DispatchAsyncAfter(int od, TimeInterval deadline, Dispatch_Closure _Nonnull pClosure, void* _Nullable pContext)
{
    return _syscall(SC_dispatch_after, od, deadline, pClosure, pContext);
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
