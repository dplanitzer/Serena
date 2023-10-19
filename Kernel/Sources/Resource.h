//
//  Resource.h
//  Apollo
//
//  Created by Dietmar Planitzer on 10/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Resource_h
#define Resource_h

#include <klib/klib.h>
#include "Object.h"

struct _Rescon;
typedef struct _Rescon* ResconRef;

struct _Resource;
typedef struct _Resource* ResourceRef;


typedef struct _ResconClass {
    UObjectClass    super;
} ResconClass;

typedef struct _ResconClass* ResconClassRef;

#define FREAD   1
#define FWRITE  2

typedef struct _Rescon {
    UObject                 super;
    ResourceRef _Nonnull    resource;
    UInt                    options;
    Byte                    state[1];
} Rescon;


extern ErrorCode Rescon_Create(ResourceRef pResource, UInt options, ByteCount stateSize, ResconRef _Nullable * _Nonnull pOutRescon);

#define Rescon_GetStateAs(__pRes, __classType) \
    ((__classType*) &__pRes->state[0])


////////////////////////////////////////////////////////////////////////////////


// Opens a resource context/channel to the resource. This new resource context
// will be represented by a (file) descriptor in user space. The resource context
// maintains state that is specific to this connection. This state will be
// protected by the resource's internal locking mechanism.
typedef FuncN(ErrorCode, Func_Resource_Open, const Character* _Nonnull pPath, UInt options, ResconRef _Nullable * _Nonnull pOutRescon);

typedef FuncN(ByteCount, Func_Resource_Read, void* pContext, Byte* _Nonnull pBuffer, ByteCount nBytesToRead);
typedef FuncN(ByteCount, Func_Resource_Write, void* pContext, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite);

// See UObject.close()
typedef FuncN(ErrorCode, Func_Resource_Close, void* pContext);

typedef struct _ResourceClass {
    Class                           super;
    Func_Resource_Open _Nullable    open;
    Func_Resource_Read _Nullable    read;
    Func_Resource_Write _Nullable   write;
    Func_Resource_Close _Nullable   close;
} ResourceClass;

typedef struct _ResourceClass* ResourceClassRef;


typedef struct _Resource {
    Object  super;
} Resource;

#define Resource_Open(__pRes, __pPath, __options, __pOutRescon) \
    Object_GetClassAs(__pRes, Resource)->open(__pRes, __pPath, __options, __pOutRescon)

#define Resource_Close(__pRes, __pCtx) \
    Object_GetClassAs(__pRes, Resource)->close(__pRes, __pCtx)

#define Resource_Read(__pRes, __pCtx, __pBuffer, __nBytesToRead) \
    Object_GetClassAs(__pRes, Resource)->read(__pRes, __pCtx, __pBuffer, __nBytesToRead)

#define Resource_Write(__pRes, __pCtx, __pBuffer, __nBytesToWrite) \
    Object_GetClassAs(__pRes, Resource)->write(__pRes, __pCtx, __pBuffer, __nBytesToWrite)
    
#endif /* Resource_h */
