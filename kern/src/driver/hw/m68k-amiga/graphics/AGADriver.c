//
//  AGADriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "AGADriverPriv.h"
#include "video_conf.h"
#include <ext/math.h>
#include <string.h>
#include <handler/IOGraphicsHandler.h>

IOCATS_DEF(g_cats, IOVID_FB);


// Creates a graphics driver instance which manages the on-board video hardware.
// We assume that video is turned off at the time this function is called and
// video remains turned off until a screen has been created and is made the
// current screen.
errno_t AGADriver_Create(AGADriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AGADriverRef self;
    
    try(IODriver_Create(class(AGADriver), g_cats, (IODriverRef*)&self));

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t AGADriver_start(AGADriverRef _Nonnull self)
{
    return gdInit();
}

bool AGADriver_isExclusive(AGADriverRef _Nonnull self)
{
    return false;
}

errno_t AGADriver_getDFSInfo(AGADriverRef _Nonnull self, IODFSInfo* _Nonnull info)
{
    strcpy(info->name, "fb");
    info->func = IOGraphicsHandler_Create;
    info->uid = UID_ROOT;
    info->gid = GID_ROOT;
    info->perms = fs_perms_from_octal(0666);

    return EOK;
}


//
// CLUT
//

errno_t AGADriver_CreateFramebuffer(AGADriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId)
{
    gdLock();
    const errno_t err = gdGenFramebuffer(colorDepth, pOutId);
    gdUnlock();
    return err;
}

errno_t AGADriver_DestroyFramebuffer(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdDeleteFramebuffer(id);
    gdUnlock();
    return err;
}

errno_t AGADriver_AttachBuffer(AGADriverRef _Nonnull self, int fb_id, int buf_id)
{
    gdLock();
    const errno_t err = gdAttachBuffer(fb_id, buf_id);
    gdUnlock();
    return err;
}

errno_t AGADriver_GetFramebufferInfo(AGADriverRef _Nonnull self, int id, vio_clut_info_t* _Nonnull pOutInfo)
{
    gdLock();
    const errno_t err = gdGetFramebufferInfo(id, pOutInfo);
    gdUnlock();
    return err;
}


//
// Pixel Buffer
//

errno_t AGADriver_CreateBuffer(AGADriverRef _Nonnull self, int width, int height, vio_pixfmt_t pixelFormat, int* _Nonnull pOutId)
{
    gdLock();
    const errno_t err = gdGenBuffer(width, height, pixelFormat, pOutId);
    gdUnlock();
    return err;
}

errno_t AGADriver_DestroyBuffer(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdDeleteBuffer(id);
    gdUnlock();
    return err;
}

errno_t AGADriver_GetBufferInfo(AGADriverRef _Nonnull self, int id, vio_buffer_info_t* _Nonnull pOutInfo)
{
    gdLock();
    const errno_t err = gdGetBufferInfo(id, pOutInfo);
    gdUnlock();
    return err;
}

errno_t AGADriver_MapBuffer(AGADriverRef _Nonnull self, int id, int mode, vio_buffer_data_t* _Nonnull pOutMapping)
{
    gdLock();
    const errno_t err = gdMapBuffer(id, mode, pOutMapping);
    gdUnlock();
    return err;
}

errno_t AGADriver_UnmapBuffer(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdUnmapBuffer(id);
    gdUnlock();
    return err;
}


//
// Sprites
//

void AGADriver_GetSpriteCaps(AGADriverRef _Nonnull self, vio_sprite_caps_t* _Nonnull cp)
{
    gdLock();
    gdGetSpriteCaps(cp);
    gdUnlock();
}


//
// Command Buffers
//

errno_t AGADriver_CreateCommandBuffer(AGADriverRef _Nonnull self, size_t size, vio_cmdbuf_desc_t* _Nonnull desc)
{
    gdLock();
    const errno_t err = gdGenCmdbuf(size, desc);
    gdUnlock();
    return err;
}

errno_t AGADriver_DestroyCommandBuffer(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdDeleteCmdbuf(id);
    gdUnlock();
    return err;
}

errno_t AGADriver_ExecuteCommandBuffer(AGADriverRef _Nonnull self, int id, size_t offset)
{
    gdLock();
    const errno_t err = gdExecCmdbuf(id, offset);
    gdUnlock();
    return err;
}


//
// In-kernel command buffer utilities
//

void* _Nonnull vio_set_clut_rgb32(void* _Nonnull addr, int clut_id, size_t idx, size_t count, const vio_rgb32_t* _Nonnull entries)
{
    struct vio_op_clut_rgb32* p = addr;

    p->opcode = VIO_OPCODE_CLUT_RGB32;
    p->clutId = clut_id;
    p->idx = idx;
    p->count = count;
    
    for (size_t i = 0; i < count; i++) {
        p->color[i] = entries[i];
    }

    return (char*)addr + sizeof(struct vio_op_clut_rgb32) + (count - 1) * sizeof(vio_rgb32_t);
}

void* _Nonnull vio_write_pixels(void* _Nonnull addr, int buf_id, const void* _Nonnull planes[], size_t bytesPerRow, vio_pixfmt_t format)
{
    struct vio_op_write_pixels* p = addr;
    const size_t pcnt = PixelFormat_GetPlaneCount(format);

    p->opcode = VIO_OPCODE_WRITE_PIXELS;
    p->bufferId = buf_id;
    p->bytesPerRow = bytesPerRow;
    p->format = format;
    
    for (size_t i = 0; i < pcnt; i++) {
        p->plane[i] = planes[i];
    }

    return (char*)addr + sizeof(struct vio_op_write_pixels) + (pcnt - 1) * sizeof(void*);
}

void* _Nonnull vio_clear_pixels(void* _Nonnull addr, int buf_id)
{
    struct vio_op_buffer* p = addr;

    p->opcode = VIO_OPCODE_CLEAR_PIXELS;
    p->bufferId = buf_id;

    return (char*)addr + sizeof(struct vio_op_buffer);
}

void* _Nonnull vio_bind_buffer(void* _Nonnull addr, int target, int buf_id)
{
    struct vio_op_bind_buffer* p = addr;

    p->opcode = VIO_OPCODE_BIND_BUFFER;
    p->target = target;
    p->bufferId = buf_id;

    return (char*)addr + sizeof(struct vio_op_bind_buffer);
}

void* _Nonnull vio_put_sprite(void* _Nonnull addr, int spr_id, int16_t x, int16_t y)
{
    struct vio_op_put_sprite* p = addr;

    p->opcode = VIO_OPCODE_PUT_SPRITE;
    p->spriteId = spr_id;
    p->x = x;
    p->y = y;

    return (char*)addr + sizeof(struct vio_op_put_sprite);
}

void* _Nonnull vio_show_sprite(void* _Nonnull addr, int spr_id, bool isVisible)
{
    struct vio_op_show_sprite* p = addr;

    p->opcode = VIO_OPCODE_SHOW_SPRITE;
    p->spriteId = spr_id;
    p->visible = isVisible;
    
    return (char*)addr + sizeof(struct vio_op_show_sprite);
}

void* _Nonnull vio_end(void* _Nonnull addr)
{
    vio_opcode_t* p = addr;

    *p = VIO_OPCODE_END;
    return (char*)addr + sizeof(vio_opcode_t);
}


//
// Screen
//

errno_t AGADriver_SetScreenConfig(AGADriverRef _Nonnull self, const intptr_t* _Nullable conf)
{
    gdLock();
    const errno_t err = gdSetScreenConfig(conf);
    gdUnlock();
    return err;
}

errno_t AGADriver_GetScreenConfig(AGADriverRef _Nonnull self, intptr_t* _Nonnull conf, size_t bufsiz)
{
    gdLock();
    const errno_t err = gdGetScreenConfig(conf, bufsiz);
    gdUnlock();
    return err;
}


//
// Video Mode
//

errno_t AGADriver_SetVideoMode(AGADriverRef _Nonnull self, const vio_mode_t* _Nonnull mode, int* _Nonnull pOutBufferId, int* _Nonnull pOutFbId)
{
    decl_try_err();
    int buf_id, fb_id;
    const bool isIndexed = (mode->pixelFormat >= VIO_COLOR_INDEX1 && mode->pixelFormat <= VIO_COLOR_INDEX8);

    gdLock();
    try(gdGenBuffer(mode->width, mode->height, mode->pixelFormat, &buf_id));
    gdClearPixels(buf_id);


    try(gdGenFramebuffer(32, &fb_id));
    try(gdAttachBuffer(fb_id, buf_id));

    if (isIndexed) {
        gdSetClutEntries(fb_id, 0, __min(32, mode->paletteSize), mode->palette);
    }


    err = gdSetCurrentFramebuffer(fb_id);
    *pOutFbId = fb_id;
    *pOutBufferId = buf_id;

catch:
    gdUnlock();
    return err;
}

void AGADriver_SetVideoOff(AGADriverRef _Nonnull self)
{
    gdLock();
    gdSetCurrentFramebuffer(0);
    gdUnlock();
}


class_func_defs(AGADriver, IODriver,
override_func_def(start, AGADriver, IODriver)
override_func_def(isExclusive, AGADriver, IODriver)
override_func_def(getDFSInfo, AGADriver, IODriver)
);
