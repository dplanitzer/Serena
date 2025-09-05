//
//  GraphicsDriver_Surface.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"


errno_t _GraphicsDriver_CreateSurface(GraphicsDriverRef _Nonnull _Locked self, int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSurface)
{
    Surface* srf;

    const errno_t err = Surface_Create(_GraphicsDriver_GetNewGObjId(self), width, height, pixelFormat, &srf);
    if (err == EOK) {
        List_InsertBeforeFirst(&self->gobjs, GObject_GetChainPtr(srf));
        *pOutSurface = srf;
    }

    return err;
}

errno_t GraphicsDriver_CreateSurface(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int* _Nonnull pOutId)
{
    Surface* srf;

    mtx_lock(&self->io_mtx);
    const errno_t err = _GraphicsDriver_CreateSurface(self, width, height, pixelFormat, &srf);
    if (err == EOK) {
        *pOutId = GObject_GetId(srf);
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_DestroySurface(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);

    if (srf) {
        if (!GObject_InUse(srf)) {
            _GraphicsDriver_DestroyGObj(self, srf);
        }
        else {
            err = EBUSY;
        }
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_GetSurfaceInfo(GraphicsDriverRef _Nonnull self, int id, SurfaceInfo* _Nonnull pOutInfo)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);

    if (srf) {
        pOutInfo->width = Surface_GetWidth(srf);
        pOutInfo->height = Surface_GetHeight(srf);
        pOutInfo->pixelFormat = Surface_GetPixelFormat(srf);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return EOK;
}

errno_t GraphicsDriver_MapSurface(GraphicsDriverRef _Nonnull self, int id, MapPixels mode, SurfaceMapping* _Nonnull pOutMapping)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);
    if (srf) {
        err = Surface_Map(srf, mode, pOutMapping);
    }
    else {
        err = ENOTSUP;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_UnmapSurface(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);
    if (srf) {
        err = Surface_Unmap(srf);
    }
    else {
        err = ENOTSUP;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}
