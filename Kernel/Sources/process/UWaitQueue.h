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
#include <kpi/signal.h>
#include <dispatcher/WaitQueue.h>


open_class(UWaitQueue, UResource,
    WaitQueue       wq;
    unsigned int    policy;
);
open_class_funcs(UWaitQueue, UResource,
);

extern errno_t UWaitQueue_Create(int policy, UWaitQueueRef _Nullable * _Nonnull pOutSelf);

extern errno_t UWaitQueue_Wait(UWaitQueueRef _Nonnull self);
extern errno_t UWaitQueue_SigWait(UWaitQueueRef _Nonnull self, const sigset_t* _Nullable mask, sigset_t* _Nonnull pOutSigs);
extern errno_t UWaitQueue_TimedWait(UWaitQueueRef _Nonnull self, int flags, const struct timespec* _Nonnull wtp);
extern errno_t UWaitQueue_SigTimedWait(UWaitQueueRef _Nonnull self, const sigset_t* _Nullable mask, sigset_t* _Nonnull pOutSigs, int flags, const struct timespec* _Nonnull wtp);
extern void UWaitQueue_Wakeup(UWaitQueueRef _Nonnull self, int flags, int signo);

#endif /* UWaitQueue_h */
