//
//  UDispatchQueue.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/16/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef UDispatchQueue_h
#define UDispatchQueue_h

#include <UResource.h>
#include <dispatchqueue/DispatchQueue.h>


open_class_with_ref(UDispatchQueue, UResource,
    DispatchQueueRef    dispatchQueue;
);
open_class_funcs(UDispatchQueue, Object,
);


extern errno_t UDispatchQueue_Create(int minConcurrency, int maxConcurrency, int qos, int priority, VirtualProcessorPoolRef _Nonnull vpPoolRef, ProcessRef _Nullable _Weak pProc, UDispatchQueueRef _Nullable * _Nonnull pOutSelf);

#endif /* UDispatchQueue_h */
