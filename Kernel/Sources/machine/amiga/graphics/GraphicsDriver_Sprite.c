//
//  GraphicsDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"
#include "copper.h"
#include <kern/kalloc.h>
#include <kpi/hid.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprites
////////////////////////////////////////////////////////////////////////////////

#define MAKE_SPRITE_ID(__sprIdx) \
((__sprIdx) + 1)

#define GET_SPRITE_IDX(__sprId) \
((__sprId) - 1)


// Called when the position or visibility of a hardware sprite has changed.
// Recalculates the sprxpos and sprxctl control words and updates them in the
// sprite DMA data block.
static void _update_sprite_ctrl_words(Sprite* _Nonnull self)
{
    // Hiding a sprite means to move it all the way to X max.
    int x = self->x;
    int y = self->y;
    int ye = y + self->height;

    if (ye > MAX_SPRITE_VPOS || ye < y) {
        ye = MAX_SPRITE_VPOS;
        y = ye - self->height;
    }

    self->data[0] = ((y & 0x00ff) << 8) | ((x & 0x01fe) >> 1);
    self->data[1] = ((ye & 0x00ff) << 8) | (((y >> 8) & 0x0001) << 2) | (((ye >> 8) & 0x0001) << 1) | (x & 0x0001);
}


errno_t _acquire_sprite(GraphicsDriverRef _Nonnull _Locked self, int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutSpriteId)
{
    decl_try_err();

    if (priority < 0 || priority >= SPRITE_COUNT) {
        return ENOTSUP;
    }
    if (width != SPRITE_WIDTH || height < 0 || height > MAX_SPRITE_HEIGHT) {
        return EINVAL;
    }
    if (pixelFormat != kPixelFormat_RGB_Indexed2) {
        return ENOTSUP;
    }

    Sprite* spr = &self->sprite[priority];
    if (spr->isAcquired) {
        throw(EBUSY);
    }


    // Set the initial sprite position to the top-left corner of the current
    // display window.
    const video_conf_t* vc = g_copper_running_prog->video_conf;
    spr->x = vc->hSprOrigin - 1;
    spr->y = vc->vSprOrigin;
    spr->height = (uint16_t)height;

    const size_t byteCount = (2 + 2*height + 2) * sizeof(uint16_t);
    try(kalloc_options(byteCount, KALLOC_OPTION_UNIFIED | KALLOC_OPTION_CLEAR, (void**)&spr->data));

    _update_sprite_ctrl_words(spr);
    spr->isAcquired = true;

    self->spriteDmaPtr[priority] = self->sprite[priority].data;

    copper_prog_t prog = _GraphicsDriver_GetEditableCopperProg(self);
    if (prog) {
        copper_prog_sprptr_changed(prog, priority, self->spriteDmaPtr[priority]);
        copper_schedule(prog, 0);
    }

catch:
    *pOutSpriteId = (err == EOK) ? MAKE_SPRITE_ID(priority) : 0;
    return err;
}

errno_t _relinquish_sprite(GraphicsDriverRef _Nonnull _Locked self, int spriteId)
{
    const int sprIdx = GET_SPRITE_IDX(spriteId);
    if (sprIdx < 0) {
        return EOK;
    }
    if (sprIdx >= SPRITE_COUNT) {
        return EINVAL;
    }


    Sprite* spr = &self->sprite[sprIdx];
    if (!spr->isAcquired) {
        return EINVAL;
    }


#if 0
    kfree(spr->data);
    spr->data = NULL;
#endif
    spr->isAcquired = false;
    spr->x = 0;
    spr->y = 0;
    spr->height = 0;

    // XXX actually free the old sprite instead of leaking it. Can't do this
    // XXX yet because we need to ensure that the DMA is no longer accessing
    // XXX the data before it freeing it.
    self->spriteDmaPtr[sprIdx] = self->nullSpriteData;
    copper_prog_t prog = _GraphicsDriver_GetEditableCopperProg(self);
    if (prog) {
        copper_prog_sprptr_changed(prog, sprIdx, self->spriteDmaPtr[sprIdx]);
        copper_schedule(prog, 0);
    }

    return EOK;
}

errno_t _set_sprite_pixels(GraphicsDriverRef _Nonnull _Locked self, int spriteId, const uint16_t* _Nonnull planes[2])
{
    const int sprIdx = GET_SPRITE_IDX(spriteId);
    if (sprIdx < 0 || sprIdx >= SPRITE_COUNT) {
        return EINVAL;
    }

    Sprite* spr = &self->sprite[sprIdx];
    if (!spr->isAcquired) {
        return EINVAL;
    }

    const uint16_t* sp0 = planes[0];
    const uint16_t* sp1 = planes[1];
    uint16_t* dp = &spr->data[2];

    for (uint16_t i = 0; i < spr->height; i++) {
        *dp++ = *sp0++;
        *dp++ = *sp1++;
    }
    *dp++ = 0;
    *dp   = 0;

    return EOK;
}

errno_t _set_sprite_pos(GraphicsDriverRef _Nonnull _Locked self, int spriteId, int x, int y)
{
    const int sprIdx = GET_SPRITE_IDX(spriteId);
    if (sprIdx < 0 || sprIdx >= SPRITE_COUNT) {
        return EINVAL;
    }


    Sprite* spr = &self->sprite[sprIdx];
    if (!spr->isAcquired) {
        return EINVAL;
    }

    const video_conf_t * vc = g_copper_running_prog->video_conf;
    const int16_t sprX = vc->hSprOrigin - 1 + (x >> vc->hSprScale);
    const int16_t sprY = vc->vSprOrigin + (y >> vc->vSprScale);

    spr->x = __max(__min(sprX, MAX_SPRITE_HPOS), 0);
    spr->y = __max(__min(sprY, MAX_SPRITE_VPOS), 0);
    _update_sprite_ctrl_words(spr);

    return EOK;
}

errno_t _set_sprite_vis(GraphicsDriverRef _Nonnull _Locked self, int spriteId, bool isVisible)
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        self->spriteDmaPtr[sprIdx] = (isVisible) ? self->sprite[sprIdx].data : self->nullSpriteData;
        copper_prog_t prog = _GraphicsDriver_GetEditableCopperProg(self);
        if (prog) {
            copper_prog_sprptr_changed(prog, sprIdx, self->spriteDmaPtr[sprIdx]);
            copper_schedule(prog, 0);
        }
    }
    else {
        err = EINVAL;
    }
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprite API
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutSpriteId)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    if (priority == MOUSE_SPRITE_PRI && self->mouseCursorId != 0) {
        err = EBUSY; 
    }
    else {
        err = _acquire_sprite(self, width, height, pixelFormat, priority, pOutSpriteId);
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull self, int spriteId)
{
    mtx_lock(&self->io_mtx);
    const errno_t err = _relinquish_sprite(self, spriteId);
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_SetSpritePixels(GraphicsDriverRef _Nonnull self, int spriteId, const uint16_t* _Nonnull planes[2])
{
    mtx_lock(&self->io_mtx);
    const errno_t err = _set_sprite_pixels(self, spriteId, planes);
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, int spriteId, int x, int y)
{
    mtx_lock(&self->io_mtx);
    const errno_t err = _set_sprite_pos(self, spriteId, x, y);
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, int spriteId, bool isVisible)
{
    mtx_lock(&self->io_mtx);
    const errno_t err = _set_sprite_vis(self, spriteId, isVisible);
    mtx_unlock(&self->io_mtx);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_AcquireMouseCursor(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat)
{
    if (width != kCursor_Width || height != kCursor_Height || pixelFormat != kCursor_PixelFormat) {
        return ENOTSUP;
    }

    mtx_lock(&self->io_mtx);
    const errno_t err = _acquire_sprite(self, width, height, pixelFormat, MOUSE_SPRITE_PRI, &self->mouseCursorId);
    mtx_unlock(&self->io_mtx);
    return err;
}

void GraphicsDriver_RelinquishMouseCursor(GraphicsDriverRef _Nonnull self)
{
    mtx_lock(&self->io_mtx);
    if (self->mouseCursorId != 0) {
        _relinquish_sprite(self, self->mouseCursorId);
        self->mouseCursorId = 0;
    }
    mtx_unlock(&self->io_mtx);
}

errno_t GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull self, const uint16_t* _Nullable planes[2])
{
    mtx_lock(&self->io_mtx);
    const errno_t err = _set_sprite_pixels(self, self->mouseCursorId, planes);
    mtx_unlock(&self->io_mtx);
    return err;
}

void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, int x, int y)
{
    mtx_lock(&self->io_mtx);
    _set_sprite_pos(self, self->mouseCursorId, x, y);
    mtx_unlock(&self->io_mtx);
}

void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull self, bool isVisible)
{
    mtx_lock(&self->io_mtx);
    _set_sprite_vis(self, self->mouseCursorId, isVisible);
    mtx_unlock(&self->io_mtx);
}
