//
//  UWaitQueue.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef UWaitQueue_h
#define UWaitQueue_h

#include <process/UResource.h>
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

#endif /* UWaitQueue_h */
