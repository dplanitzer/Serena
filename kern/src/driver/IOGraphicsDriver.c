//
//  IOGraphicsDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/12/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IOGraphicsDriver.h"


bool IOGraphicsDriver_isExclusive(IOGraphicsDriverRef _Nonnull self)
{
    return false;
}



//
// Images
//

errno_t IOGraphicsDriver_createImage(IOGraphicsDriverRef _Nonnull self, int width, int height, gd_pixfmt_t pixelFormat, int* _Nonnull pOutId)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_destroyImage(IOGraphicsDriverRef _Nonnull self, int id)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_getImageInfo(IOGraphicsDriverRef _Nonnull self, int id, gd_image_info_t* _Nonnull pOutInfo)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_mapImage(IOGraphicsDriverRef _Nonnull self, int id, int mode, gd_image_data_t* _Nonnull pOutMapping)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_unmapImage(IOGraphicsDriverRef _Nonnull self, int id)
{
    return ENOTSUP;
}


//
// Sprites
//

errno_t IOGraphicsDriver_acquireSprites(IOGraphicsDriverRef _Nonnull self, int basePriority, size_t count, int* _Nonnull pOutSpriteIds)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_releaseSprites(IOGraphicsDriverRef _Nonnull self, const int* _Nullable spriteIds, size_t count)
{
    return ENOTSUP;
}

void IOGraphicsDriver_getSpriteConstraints(IOGraphicsDriverRef _Nonnull self, gd_sprite_constraints_t* _Nonnull cp)
{
}


//
// CLUT
//

errno_t IOGraphicsDriver_clut(IOGraphicsDriverRef _Nonnull self, size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_getClut(IOGraphicsDriverRef _Nonnull self, size_t idx, size_t count, gd_rgb32_t* _Nonnull entries)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_getClutInfo(IOGraphicsDriverRef _Nonnull self, gd_clut_info_t* _Nonnull info)
{
    return ENOTSUP;
}


//
// Display
//

errno_t IOGraphicsDriver_displayMode(IOGraphicsDriverRef _Nonnull self, const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_getDisplayInfo(IOGraphicsDriverRef _Nonnull self, int flavor, gd_display_info_ref_t _Nonnull info)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_enumDisplayModes(IOGraphicsDriverRef _Nonnull self, int index, gd_display_mode_t* _Nonnull pOutMode)
{
    return ENOTSUP;
}


//
// Command Buffers
//

errno_t IOGraphicsDriver_createCommandBuffer(IOGraphicsDriverRef _Nonnull self, size_t size, gd_cmdbuf_desc_t* _Nonnull desc)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_destroyCommandBuffer(IOGraphicsDriverRef _Nonnull self, int id)
{
    return ENOTSUP;
}

errno_t IOGraphicsDriver_submitCommandBuffer(IOGraphicsDriverRef _Nonnull self, int queue_id, int cmds_id)
{
    return ENOTSUP;
}


//
// In-kernel command buffer utilities (XXX tmp)
//
extern int8_t _gdGetPlaneCount(gd_pixfmt_t format);

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
    p->dstImageId = img_id;
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
    p->dstImageId = img_id;

    return (char*)addr + sizeof(struct gd_op_clear_pixels);
}


void* _Nonnull gdCmdBindSpriteImage(void* _Nonnull addr, int spr_id, int img_id)
{
    struct gd_op_sprite_image* p = addr;

    p->opcode = GD_OPCODE_SPRITE_IMAGE;
    p->spriteId = spr_id;
    p->imageId = img_id;

    return (char*)addr + sizeof(struct gd_op_sprite_image);
}

void* _Nonnull gdCmdMoveSprite(void* _Nonnull addr, int spr_id, int16_t x, int16_t y)
{
    struct gd_op_sprite_move* p = addr;

    p->opcode = GD_OPCODE_SPRITE_MOVE;
    p->spriteId = spr_id;
    p->x = x;
    p->y = y;

    return (char*)addr + sizeof(struct gd_op_sprite_move);
}

void* _Nonnull gdCmdShowSprite(void* _Nonnull addr, int spr_id, bool isVisible)
{
    struct gd_op_sprite_show* p = addr;

    p->opcode = GD_OPCODE_SPRITE_SHOW;
    p->spriteId = spr_id;
    p->visible = isVisible;
    
    return (char*)addr + sizeof(struct gd_op_sprite_show);
}


class_func_defs(IOGraphicsDriver, IODriver,
override_func_def(isExclusive, IOGraphicsDriver, IODriver)
func_def(createImage, IOGraphicsDriver)
func_def(destroyImage, IOGraphicsDriver)
func_def(getImageInfo, IOGraphicsDriver)
func_def(mapImage, IOGraphicsDriver)
func_def(unmapImage, IOGraphicsDriver)
func_def(acquireSprites, IOGraphicsDriver)
func_def(releaseSprites, IOGraphicsDriver)
func_def(getSpriteConstraints, IOGraphicsDriver)
func_def(createCommandBuffer, IOGraphicsDriver)
func_def(destroyCommandBuffer, IOGraphicsDriver)
func_def(submitCommandBuffer, IOGraphicsDriver)
func_def(clut, IOGraphicsDriver)
func_def(getClut, IOGraphicsDriver)
func_def(getClutInfo, IOGraphicsDriver)
func_def(displayMode, IOGraphicsDriver)
func_def(getDisplayInfo, IOGraphicsDriver)
func_def(enumDisplayModes, IOGraphicsDriver)
);
