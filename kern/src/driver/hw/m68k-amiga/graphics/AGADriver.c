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
#include <process/Process.h>

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

void AGADriver_doClose(IODriverRef _Nonnull _Locked self)
{
    IODriver_Super_DoClose(AGADriver);

    gdLock();
    gdDestroyImagesOwnedBy(Process_GetCurrentId());
    gdUnlock();
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
// Images
//

errno_t AGADriver_CreateImage(AGADriverRef _Nonnull self, int width, int height, gd_pixfmt_t pixelFormat, int* _Nonnull pOutId)
{
    gdLock();
    const errno_t err = gdCreateImage(Process_GetCurrentId(), width, height, pixelFormat, pOutId);
    gdUnlock();
    return err;
}

errno_t AGADriver_DestroyImage(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdDestroyImage(Process_GetCurrentId(), id);
    gdUnlock();
    return err;
}

errno_t AGADriver_GetImageInfo(AGADriverRef _Nonnull self, int id, gd_image_info_t* _Nonnull pOutInfo)
{
    gdLock();
    const errno_t err = gdGetImageInfo(Process_GetCurrentId(), id, pOutInfo);
    gdUnlock();
    return err;
}

errno_t AGADriver_MapImage(AGADriverRef _Nonnull self, int id, int mode, gd_image_data_t* _Nonnull pOutMapping)
{
    gdLock();
    const errno_t err = gdMapImage(Process_GetCurrentId(), id, mode, pOutMapping);
    gdUnlock();
    return err;
}

errno_t AGADriver_UnmapImage(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdUnmapImage(Process_GetCurrentId(), id);
    gdUnlock();
    return err;
}


//
// Sprites
//

void AGADriver_GetSpriteCaps(AGADriverRef _Nonnull self, gd_sprite_caps_t* _Nonnull cp)
{
    gdLock();
    gdGetSpriteCaps(cp);
    gdUnlock();
}


//
// CLUT
//

errno_t AGADriver_Clut(AGADriverRef _Nonnull self, size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries)
{
    gdLock();
    const errno_t err = gdClut(idx, count, entries);
    gdUnlock();
    return err;
}

errno_t AGADriver_GetClut(AGADriverRef _Nonnull self, size_t idx, size_t count, gd_rgb32_t* _Nonnull entries)
{
    gdLock();
    const errno_t err = gdGetClut(idx, count, entries);
    gdUnlock();
    return err;
}

errno_t AGADriver_GetClutInfo(AGADriverRef _Nonnull self, gd_clut_info_t* _Nonnull info)
{
    gdLock();
    const errno_t err = gdGetClutInfo(info);
    gdUnlock();
    return err;
}


//
// Display
//

errno_t AGADriver_DisplayMode(AGADriverRef _Nonnull self, const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op)
{
    gdLock();
    const errno_t err = gdDisplayMode(mode, params, op);
    gdUnlock();
    return err;
}

errno_t AGADriver_GetDisplayInfo(AGADriverRef _Nonnull self, int flavor, gd_display_info_ref_t _Nonnull info)
{
    gdLock();
    const errno_t err = gdGetDisplayInfo(flavor, info);
    gdUnlock();
    return err;
}

errno_t AGADriver_EnumDisplayModes(AGADriverRef _Nonnull self, int index, gd_display_mode_t* _Nonnull pOutMode)
{
    gdLock();
    const errno_t err = gdEnumDisplayModes(index, pOutMode);
    gdUnlock();
    return err;
}


//
// Command Buffers
//

errno_t AGADriver_CreateCommandBuffer(AGADriverRef _Nonnull self, size_t size, gd_cmdbuf_desc_t* _Nonnull desc)
{
    gdLock();
    const errno_t err = gdCreateCommandBuffer(Process_GetCurrentId(), size, desc);
    gdUnlock();
    return err;
}

errno_t AGADriver_DestroyCommandBuffer(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdDestroyCommandBuffer(Process_GetCurrentId(), id);
    gdUnlock();
    return err;
}

errno_t AGADriver_SubmitCommandBuffer(AGADriverRef _Nonnull self, int queue_id, int cmds_id)
{
    gdLock();
    const errno_t err = gdSubmitCommandBuffer(Process_GetCurrentId(), queue_id, cmds_id);
    gdUnlock();
    return err;
}


//
// In-kernel command buffer utilities
//

void* _Nonnull gdCmdEnd(void* _Nonnull addr)
{
    gd_opcode_t* p = addr;

    *p = GD_OPCODE_END;
    return (char*)addr + sizeof(gd_opcode_t);
}


void* _Nonnull gdCmdWritePixels(void* _Nonnull addr, int img_id, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format)
{
    struct gd_op_write_pixels* p = addr;
    const size_t pcnt = _gdGetPlaneCount(format);

    p->opcode = GD_OPCODE_WRITE_PIXELS;
    p->dstBufferId = img_id;
    p->bytesPerRow = bytesPerRow;
    p->format = format;
    
    for (size_t i = 0; i < pcnt; i++) {
        p->plane[i] = planes[i];
    }

    return (char*)addr + sizeof(struct gd_op_write_pixels) + (pcnt - 1) * sizeof(void*);
}

void* _Nonnull gdCmdClearPixels(void* _Nonnull addr, int img_id)
{
    struct gd_op_clear_pixels* p = addr;

    p->opcode = GD_OPCODE_CLEAR_PIXELS;
    p->dstBufferId = img_id;

    return (char*)addr + sizeof(struct gd_op_clear_pixels);
}


void* _Nonnull gdCmdBindSpriteImage(void* _Nonnull addr, int target, int img_id)
{
    struct gd_op_bind_image* p = addr;

    p->opcode = GD_OPCODE_BIND_IMAGE;
    p->target = target;
    p->bufferId = img_id;

    return (char*)addr + sizeof(struct gd_op_bind_image);
}

void* _Nonnull gdCmdMoveSprite(void* _Nonnull addr, int spr_id, int16_t x, int16_t y)
{
    struct gd_op_move_sprite* p = addr;

    p->opcode = GD_OPCODE_MOVE_SPRITE;
    p->spriteId = spr_id;
    p->x = x;
    p->y = y;

    return (char*)addr + sizeof(struct gd_op_move_sprite);
}

void* _Nonnull gdCmdShowSprite(void* _Nonnull addr, int spr_id, bool isVisible)
{
    struct gd_op_show_sprite* p = addr;

    p->opcode = GD_OPCODE_SHOW_SPRITE;
    p->spriteId = spr_id;
    p->visible = isVisible;
    
    return (char*)addr + sizeof(struct gd_op_show_sprite);
}


class_func_defs(AGADriver, IODriver,
override_func_def(start, AGADriver, IODriver)
override_func_def(isExclusive, AGADriver, IODriver)
override_func_def(doClose, AGADriver, IODriver)
override_func_def(getDFSInfo, AGADriver, IODriver)
);
