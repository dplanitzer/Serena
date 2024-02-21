//
//  VirtualProcessorPool.h
//  Apollo
//
//  Created by Dietmar Planitzer on 4/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef VirtualProcessorPool_h
#define VirtualProcessorPool_h

#include <klib/klib.h>
#include "VirtualProcessor.h"


typedef struct _VirtualProcessorParameters {
    Closure1Arg_Func _Nonnull   func;
    void* _Nullable _Weak       context;
    size_t                      kernelStackSize;
    size_t                      userStackSize;
    int                         priority;
} VirtualProcessorParameters;

#define VirtualProcessorParameters_Make(__pFunc, __pContext, __kernelStackSize, __userStackSize, __priority) \
    ((VirtualProcessorParameters) {__pFunc, __pContext, __kernelStackSize, __userStackSize, __priority})


struct _VirtualProcessorPool;
typedef struct _VirtualProcessorPool* VirtualProcessorPoolRef;


extern VirtualProcessorPoolRef _Nonnull gVirtualProcessorPool;

extern errno_t VirtualProcessorPool_Create(VirtualProcessorPoolRef _Nullable * _Nonnull pOutPool);
extern void VirtualProcessorPool_Destroy(VirtualProcessorPoolRef _Nullable pool);

extern errno_t VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPoolRef _Nonnull pool, VirtualProcessorParameters params, VirtualProcessor* _Nonnull * _Nonnull pOutVP);
extern _Noreturn VirtualProcessorPool_RelinquishVirtualProcessor(VirtualProcessorPoolRef _Nonnull pool, VirtualProcessor* _Nonnull pVP);

#endif /* VirtualProcessorPool_h */
