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


typedef struct _VirtualProcessorAttributes {
    Int     kernelStackSize;
    Int     userStackSize;
    Int     priority;
} VirtualProcessorAttributes;

extern void VirtualProcessorAttributes_Init(VirtualProcessorAttributes* _Nonnull pAttribs);



struct _VirtualProcessorPool;
typedef struct _VirtualProcessorPool* VirtualProcessorPoolRef;


extern VirtualProcessorPoolRef _Nonnull VirtualProcessorPool_GetShared(void);

extern VirtualProcessorPoolRef _Nullable VirtualProcessorPool_Create(void);
extern void VirtualProcessorPool_Destroy(VirtualProcessorPoolRef _Nullable pool);

extern VirtualProcessor* _Nonnull VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPoolRef _Nonnull pool, const VirtualProcessorAttributes* _Nonnull pAttribs, VirtualProcessor_Closure _Nonnull pClosure, Byte* _Nullable pContext, Bool isUser);
extern void VirtualProcessorPool_RelinquishVirtualProcessor(VirtualProcessorPoolRef _Nonnull pool, VirtualProcessor* _Nonnull pVP);

#endif /* VirtualProcessorPool_h */
