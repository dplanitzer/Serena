//
//  VirtualProcessorPool.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef VirtualProcessorPool_h
#define VirtualProcessorPool_h

#include "VirtualProcessor.h"


typedef struct VirtualProcessorParameters {
    VoidFunc_1 _Nonnull   func;
    void* _Nullable _Weak       context;
    size_t                      kernelStackSize;
    size_t                      userStackSize;
    int                         priority;
} VirtualProcessorParameters;

#define VirtualProcessorParameters_Make(__pFunc, __pContext, __kernelStackSize, __userStackSize, __priority) \
    ((VirtualProcessorParameters) {__pFunc, __pContext, __kernelStackSize, __userStackSize, __priority})


struct VirtualProcessorPool;
typedef struct VirtualProcessorPool* VirtualProcessorPoolRef;


extern VirtualProcessorPoolRef _Nonnull gVirtualProcessorPool;

extern errno_t VirtualProcessorPool_Create(VirtualProcessorPoolRef _Nullable * _Nonnull pOutSelf);
extern void VirtualProcessorPool_Destroy(VirtualProcessorPoolRef _Nullable self);

extern errno_t VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPoolRef _Nonnull self, VirtualProcessorParameters params, VirtualProcessor* _Nonnull * _Nonnull pOutVP);
extern _Noreturn VirtualProcessorPool_RelinquishVirtualProcessor(VirtualProcessorPoolRef _Nonnull self, VirtualProcessor* _Nonnull vp);

#endif /* VirtualProcessorPool_h */
