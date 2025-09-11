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
        bool isBound = (g_copper_running_prog->res.fb == srf);

        for (int i = 0; i < SPRITE_COUNT; i++) {
            if (self->spriteChannel[i].surface == srf) {
                isBound = true;
            }
        }

        if (!isBound) {
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

    if (srf == NULL) {
        throw(EINVAL);
    }
    if ((srf->flags & kSurfaceFlag_IsMapped) == kSurfaceFlag_IsMapped) {
        throw(EBUSY);
    }
    if (Surface_GetPixelFormat(srf) == kPixelFormat_RGB_Sprite2) {
        // Disallow mapping sprite surfaces for now
        throw(ENOTSUP);
    }

    pOutMapping->planeCount = Surface_GetPlaneCount(srf);
    pOutMapping->bytesPerRow = Surface_GetBytesPerRow(srf);
    for (size_t i = 0; i < pOutMapping->planeCount; i++) {
        pOutMapping->plane[i] = Surface_GetPlane(srf, i);
    }

    srf->flags |= kSurfaceFlag_IsMapped;

catch:
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_UnmapSurface(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);
    if (srf) {
        if ((srf->flags & kSurfaceFlag_IsMapped) == kSurfaceFlag_IsMapped) {
            srf->flags &= ~kSurfaceFlag_IsMapped;
        }
        else {
            err = EPERM;
        }
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_WritePixels(GraphicsDriverRef _Nonnull self, int id, const void* _Nonnull planes[], size_t bytesPerRow, PixelFormat format)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);
    if (srf) {
        Surface_WritePixels(srf, planes, bytesPerRow, format);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_ClearPixels(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);
    if (srf) {
        Surface_ClearPixels(srf);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_BindSurface(GraphicsDriverRef _Nonnull self, int target, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = (id != 0) ? _GraphicsDriver_GetSurfaceForId(self, id) : NULL;
    if (srf || id == 0) {
        switch (target & 0xffff0000) {
            case kTarget_Sprite0:
                err = _GraphicsDriver_BindSprite(self, target & 0x0000ffff, srf);
                break;

            default:
                err = EINVAL;
                break;
        }
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}
