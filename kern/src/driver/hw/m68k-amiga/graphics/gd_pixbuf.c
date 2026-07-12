//
//  gd_pixbuf.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd.h"


errno_t gdGenBuffer(int width, int height, vio_pixfmt_t pixelFormat, int* _Nonnull pOutId)
{
    Surface* srf;

    const errno_t err = Surface_Create(width, height, pixelFormat, &srf);
    if (err == EOK) {
        *pOutId = Surface_GetId(srf);
    }
    return err;
}

errno_t gdDeleteBuffer(int id)
{
    if (id == 0) {
        return EOK;
    }

    Surface* srf = Surface_GetForId(id);

    if (srf == NULL) {
        return EINVAL;
    }
    if (Surface_IsMapped(srf) || g_copper_running_prog->res.fb == srf) {
        return EBUSY;
    }

    for (int i = 0; i < SPRITE_COUNT; i++) {
        if (g_sprite[i].surface == srf) {
            _gdBindSprite(i, NULL);
        }
    }

    Surface_DelRef(srf);

    return EOK;
}

errno_t gdGetBufferInfo(int id, vio_buffer_info_t* _Nonnull pOutInfo)
{
    Surface* srf = Surface_GetForId(id);

    if (srf == NULL) {
        return EINVAL;
    }

    pOutInfo->width = Surface_GetWidth(srf);
    pOutInfo->height = Surface_GetHeight(srf);
    pOutInfo->pixelFormat = Surface_GetPixelFormat(srf);

    return EOK;
}

errno_t gdBindBuffer(int target, int id)
{
    Surface* srf = (id != 0) ? Surface_GetForId(id) : NULL;
    
    if (srf || id == 0) {
        switch (target & 0xffff0000) {
            case VIO_SPRITE_0:
                return _gdBindSprite(target & 0x0000ffff, srf);

            default:
                return EINVAL;
        }
    }
    else {
        return EINVAL;
    }
}

errno_t gdMapBuffer(int id, int mode, vio_buffer_data_t* _Nonnull pOutMapping)
{
    Surface* srf = Surface_GetForId(id);

    if (srf == NULL) {
        return EINVAL;
    }
    if (Surface_IsMapped(srf)) {
        return EBUSY;
    }
    if (Surface_GetPixelFormat(srf) == VIO_RGB_SPRITE_2) {
        // Disallow mapping sprite surfaces for now
        return ENOTSUP;
    }

    pOutMapping->planeCount = Surface_GetPlaneCount(srf);
    pOutMapping->bytesPerRow = Surface_GetBytesPerRow(srf);
    for (size_t i = 0; i < pOutMapping->planeCount; i++) {
        pOutMapping->plane[i] = Surface_GetPlane(srf, i);
    }

    srf->flags |= kSurfaceFlag_IsMapped;

    return EOK;
}

errno_t gdUnmapBuffer(int id)
{
    Surface* srf = Surface_GetForId(id);

    if (srf == NULL) {
        return EINVAL;
    }


    if (Surface_IsMapped(srf)) {
        srf->flags &= ~kSurfaceFlag_IsMapped;
        return EOK;
    }
    else {
        return EPERM;
    }
}

errno_t gdWritePixels(int id, const void* _Nonnull planes[], size_t bytesPerRow, vio_pixfmt_t format)
{
    Surface* srf = Surface_GetForId(id);

    if (srf == NULL) {
        return EINVAL;
    }

    return Surface_WritePixels(srf, planes, bytesPerRow, format);
}

errno_t gdClearPixels(int id)
{
    Surface* srf = Surface_GetForId(id);

    if (srf == NULL) {
        return EINVAL;
    }

    return Surface_ClearPixels(srf);
}
