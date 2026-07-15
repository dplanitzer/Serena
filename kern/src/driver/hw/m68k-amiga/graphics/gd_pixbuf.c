//
//  gd_pixbuf.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"


errno_t gdGenBuffer(int width, int height, vio_pixfmt_t pixelFormat, int* _Nonnull pOutId)
{
    Surface* pbo;

    const errno_t err = Surface_Create(width, height, pixelFormat, &pbo);
    if (err == EOK) {
        *pOutId = Surface_GetId(pbo);
    }
    return err;
}

errno_t gdDeleteBuffer(int id)
{
    if (id == 0) {
        return EOK;
    }


    Surface* pbo = Surface_GetForId(id);

    if (pbo == NULL) {
        return EINVAL;
    }
    if (Surface_IsMapped(pbo)) {
        return EBUSY;
    }


    // Unschedule an upcoming Copper program first, to make sure that the currently
    // running Copper program can't change on us while we're inspecting it. GD is
    // locked too so nobody else can trigger the scheduling of another Copper
    // program while we're here.
    copper_prog_t next_prog = copper_unschedule();
    bool bNeedEditableCopperProg = false;


    // We do not allow the deletion of the buffer if it is currently being used
    // as a framebuffer.
    if (g_copper_running_prog->res.pbo == pbo) {
        if (next_prog) {
            copper_schedule(next_prog, 0);
        }
        return EBUSY;
    }


    for (int i = 0; i < SPRITE_COUNT; i++) {
        if (g_sprite[i].pixbuf == pbo) {
            bNeedEditableCopperProg = true;
            break;
        }
    }

    if (bNeedEditableCopperProg && !next_prog) {
        next_prog = copper_get_editable_prog();
    }


    for (int i = 0; i < SPRITE_COUNT; i++) {
        sprite_channel_t* spr = &g_sprite[i];

        if (spr->pixbuf == pbo) {
            _bind_sprite_buffer(spr, NULL);
            copper_prog_sprptr_changed(next_prog, spr->id, (spr->pixbuf && spr->isVisible) ? spr->pixbuf : NULL);
        }
    }


    Surface_Conceal(pbo);
    Surface_DelRef(pbo);

    if (next_prog) {
        copper_schedule(next_prog, 0);
    }

    return EOK;
}

errno_t gdGetBufferInfo(int id, vio_buffer_info_t* _Nonnull pOutInfo)
{
    Surface* pbo = Surface_GetForId(id);

    if (pbo == NULL) {
        return EINVAL;
    }

    pOutInfo->width = Surface_GetWidth(pbo);
    pOutInfo->height = Surface_GetHeight(pbo);
    pOutInfo->pixelFormat = Surface_GetPixelFormat(pbo);

    return EOK;
}

errno_t gdBindBuffer(int target, int id)
{
    Surface* pbo = (id != 0) ? Surface_GetForId(id) : NULL;
    
    if (pbo || id == 0) {
        switch (target & 0xffff0000) {
            case VIO_SPRITE_0:
                return _gdBindSpriteBuffer(target & 0x0000ffff, pbo);

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
    Surface* pbo = Surface_GetForId(id);

    if (pbo == NULL) {
        return EINVAL;
    }
    if (Surface_IsMapped(pbo)) {
        return EBUSY;
    }
    if (Surface_GetPixelFormat(pbo) == VIO_RGB_SPRITE_2) {
        // Disallow mapping sprite surfaces for now
        return ENOTSUP;
    }

    pOutMapping->planeCount = Surface_GetPlaneCount(pbo);
    pOutMapping->bytesPerRow = Surface_GetBytesPerRow(pbo);
    for (size_t i = 0; i < pOutMapping->planeCount; i++) {
        pOutMapping->plane[i] = Surface_GetPlane(pbo, i);
    }

    pbo->flags |= kSurfaceFlag_IsMapped;

    return EOK;
}

errno_t gdUnmapBuffer(int id)
{
    Surface* pbo = Surface_GetForId(id);

    if (pbo == NULL) {
        return EINVAL;
    }


    if (Surface_IsMapped(pbo)) {
        pbo->flags &= ~kSurfaceFlag_IsMapped;
        return EOK;
    }
    else {
        return EPERM;
    }
}

errno_t gdWritePixels(int id, const void* _Nonnull planes[], size_t bytesPerRow, vio_pixfmt_t format)
{
    Surface* pbo = Surface_GetForId(id);

    if (pbo == NULL) {
        return EINVAL;
    }

    return Surface_WritePixels(pbo, planes, bytesPerRow, format);
}

errno_t gdClearPixels(int id)
{
    Surface* pbo = Surface_GetForId(id);

    if (pbo == NULL) {
        return EINVAL;
    }

    return Surface_ClearPixels(pbo);
}
