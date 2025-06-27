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


open_class(UWaitQueue, UResource,
    List    queue;
);
open_class_funcs(UWaitQueue, UResource,
);

extern errno_t UWaitQueue_Create(UWaitQueueRef _Nullable * _Nonnull pOutSelf);

extern errno_t UWaitQueue_Wait(UWaitQueueRef _Nonnull self);
extern errno_t UWaitQueue_TimedWait(UWaitQueueRef _Nonnull self, int options, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp);
extern void UWaitQueue_Wakeup(UWaitQueueRef _Nonnull self, int flags);

#endif /* UWaitQueue_h */
