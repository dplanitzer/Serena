//
//  dispatch.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <dispatchqueue/DispatchQueue.h>
#include <time.h>
#include <kern/limits.h>
#include <kern/timespec.h>


SYSCALL_5(dispatch, int od, const VoidFunc_2 _Nonnull func, void* _Nullable ctx, uint32_t options, uintptr_t tag)
{
    return Process_DispatchUserClosure((ProcessRef)p, pa->od, pa->func, pa->ctx, pa->options, pa->tag);
}

SYSCALL_6(dispatch_timer, int od, const struct timespec* _Nonnull deadline, const struct timespec* _Nonnull interval, const VoidFunc_1 _Nonnull func, void* _Nullable ctx, uintptr_t tag)
{
    return Process_DispatchUserTimer((ProcessRef)p, pa->od, pa->deadline, pa->interval, pa->func, pa->ctx, pa->tag);
}

SYSCALL_5(dispatch_queue_create, int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nonnull pOutQueue)
{
    return Process_CreateDispatchQueue((ProcessRef)p, pa->minConcurrency, pa->maxConcurrency, pa->qos, pa->priority, pa->pOutQueue);
}

SYSCALL_2(dispatch_remove_by_tag, int od, uintptr_t tag)
{
    return Process_DispatchRemoveByTag((ProcessRef)p, pa->od, pa->tag);
}

SYSCALL_0(dispatch_queue_current)
{
    return Process_GetCurrentDispatchQueue((ProcessRef)p);
}
