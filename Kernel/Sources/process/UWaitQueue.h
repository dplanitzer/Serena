//
//  UWaitQueue.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef UWaitQueue_h
#define UWaitQueue_h

#include <UResource.h>
#include <klib/List.h>
#include <kpi/_time.h>
#include <dispatcher/WaitQueue.h>


open_class(UWaitQueue, UResource,
    WaitQueue       wq;
    unsigned int    policy;
);
open_class_funcs(UWaitQueue, UResource,
);

extern errno_t UWaitQueue_Create(int policy, UWaitQueueRef _Nullable * _Nonnull pOutSelf);

extern errno_t UWaitQueue_Wait(UWaitQueueRef _Nonnull self);
extern errno_t UWaitQueue_TimedWait(UWaitQueueRef _Nonnull self, int options, const struct timespec* _Nonnull wtp);
extern errno_t UWaitQueue_SigWait(UWaitQueueRef _Nonnull self, unsigned int* _Nullable pOutSigs);
extern errno_t UWaitQueue_SigTimedWait(UWaitQueueRef _Nonnull self, int options, const struct timespec* _Nonnull wtp, unsigned int* _Nullable pOutSigs);
extern void UWaitQueue_Wakeup(UWaitQueueRef _Nonnull self, int flags);
extern void UWaitQueue_Signal(UWaitQueueRef _Nonnull self, int flags, unsigned int sigs);

#endif /* UWaitQueue_h */
