//
//  VirtualProcessorPool.h
//  Apollo
//
//  Created by Dietmar Planitzer on 4/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef VirtualProcessorPool_h
#define VirtualProcessorPool_h

#include "Foundation.h"
#include "List.h"
#include "VirtualProcessor.h"


typedef struct _VirtualProcessorParameters {
    VirtualProcessor_ClosureFunc _Nonnull   func;
    Byte* _Nullable _Weak                   context;
    Int                                     kernelStackSize;
    Int                                     userStackSize;
    Int                                     priority;
} VirtualProcessorParameters;

static inline VirtualProcessorParameters VirtualProcessorParameters_Make(VirtualProcessor_ClosureFunc _Nonnull pFunc, Byte* _Nullable _Weak pContext, Int kernelStackSize, Int userStackSize, Int priority) {
    VirtualProcessorParameters p;
    p.func = pFunc;
    p.context = pContext;
    p.kernelStackSize = kernelStackSize;
    p.userStackSize = userStackSize;
    p.priority = priority;
    return p;
}


struct _VirtualProcessorPool;
typedef struct _VirtualProcessorPool* VirtualProcessorPoolRef;


extern VirtualProcessorPoolRef _Nonnull VirtualProcessorPool_GetShared(void);

extern VirtualProcessorPoolRef _Nullable VirtualProcessorPool_Create(void);
extern void VirtualProcessorPool_Destroy(VirtualProcessorPoolRef _Nullable pool);

extern VirtualProcessor* _Nonnull VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPoolRef _Nonnull pool, VirtualProcessorParameters params);
extern void VirtualProcessorPool_RelinquishVirtualProcessor(VirtualProcessorPoolRef _Nonnull pool, VirtualProcessor* _Nonnull pVP);

#endif /* VirtualProcessorPool_h */
