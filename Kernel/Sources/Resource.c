//
//  Resource.c
//  Apollo
//
//  Created by Dietmar Planitzer on 10/10/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Resource.h"

ByteCount _Rescon_Read(ResconRef _Nonnull self, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    if ((self->options & FREAD) == 0) {
        return -EBADF;
    }

    if (Object_Implements(self, Resource, read)) {
        return Resource_Read(self->resource, self->state, pBuffer, nBytesToRead);
    } else {
        return -EBADF;
    }
}

ByteCount _Rescon_Write(ResconRef _Nonnull self, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite)
{
    if ((self->options & FWRITE) == 0) {
        return -EBADF;
    }

    if (Object_Implements(self, Resource, write)) {
        return Resource_Write(self->resource, self->state, pBuffer, nBytesToWrite);
    } else {
        return -EBADF;
    }
}

ErrorCode _Rescon_Close(ResconRef _Nonnull self)
{
    if (Object_Implements(self, Resource, close)) {
        return Resource_Close(self->resource, self->state);
    } else {
        return EOK;
    }
}

void _Rescon_Deinit(ResconRef _Nonnull self)
{
    Object_Release(self->resource);
    self->resource = NULL;
}


static ResconClass gResconClass = {
    (Func_Object_Deinit)_Rescon_Deinit,
    (Func_UObject_Read)_Rescon_Read,
    (Func_UObject_Write)_Rescon_Write,
    (Func_UObject_Close)_Rescon_Close
};

ErrorCode Rescon_Create(ResourceRef _Nonnull pResource, UInt options, ByteCount stateSize, ResconRef _Nullable * _Nonnull pOutRescon)
{
    decl_try_err();
    ResconRef pRescon;

    try(Object_Create(&gResconClass, sizeof(Rescon) + stateSize - 1, &pRescon));
    pRescon->resource = Object_RetainAs(pResource, Resource);
    pRescon->options = options;
    *pOutRescon = pRescon;
    return EOK;

catch:
    *pOutRescon = NULL;
    return err;
}

ErrorCode Rescon_CreateCopy(ResconRef _Nonnull pInRescon, ByteCount stateSize, ResconRef _Nullable * _Nonnull pOutRescon)
{
    decl_try_err();
    ResconRef pRescon;

    try(Object_Create(&gResconClass, sizeof(Rescon) + stateSize - 1, &pRescon));
    pRescon->resource = Object_RetainAs(pInRescon->resource, Resource);
    pRescon->options = pInRescon->options;
    Bytes_CopyRange(&pRescon->state[0], &pInRescon->state[0], stateSize);

    *pOutRescon = pRescon;
    return EOK;

catch:
    *pOutRescon = NULL;
    return err;
}
