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
static uint32_t _calc_sprite_ctl(const sprite_channel_t* _Nonnull self)
{
    uint16_t x = self->x;
    uint16_t y = self->y;
    uint16_t ye = y + self->height;

    if (ye > MAX_SPRITE_VPOS || ye < y) {
        ye = MAX_SPRITE_VPOS;
        y = ye - self->height;
    }

    const uint32_t hw = ((y & 0x00ff) << 8) | ((x & 0x01fe) >> 1);
    const uint32_t lw = ((ye & 0x00ff) << 8) | (((y >> 8) & 0x0001) << 2) | (((ye >> 8) & 0x0001) << 1) | (x & 0x0001);

    return (hw << 16) | lw;
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

    sprite_channel_t* spr = &self->spriteChannel[priority];
    if (spr->isAcquired) {
        throw(EBUSY);
    }


    Surface* srf;
    err = Surface_Create(0, width, height, kPixelFormat_RGB_Sprite2, &srf);
    if (err != EOK) {
        return err;
    }

    // Set the initial sprite position to the top-left corner of the current
    // display window.
    const video_conf_t* vc = g_copper_running_prog->video_conf;
    spr->x = vc->hSprOrigin - 1;
    spr->y = vc->vSprOrigin;
    spr->height = (uint16_t)height;
    spr->isVisible = true;
    spr->isAcquired = true;

    spr->surface = srf;
    GObject_AddRef(srf);
    Surface_ClearPixels(srf);

    uint32_t* sprptr = (uint32_t*)Surface_GetPlane(srf, 0);
    *sprptr = _calc_sprite_ctl(spr);

    copper_prog_t prog = _GraphicsDriver_GetEditableCopperProg(self);
    if (prog) {
        copper_prog_sprptr_changed(prog, priority, self->spriteChannel[priority].surface);
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


    sprite_channel_t* spr = &self->spriteChannel[sprIdx];
    if (!spr->isAcquired) {
        return EINVAL;
    }


    spr->isAcquired = false;
    spr->x = 0;
    spr->y = 0;
    spr->height = 0;

    // Cancel any still pending control word writes
    sprite_ctl_cancel(sprIdx);

    // Drop the sprite channel reference. Note that the currently running Copper
    // program still holds a reference on the sprite surface. This one will be
    // freed after the Copper program has been retired
    GObject_DelRef(spr->surface);
    spr->surface = NULL;

    copper_prog_t prog = _GraphicsDriver_GetEditableCopperProg(self);
    if (prog) {
        copper_prog_sprptr_changed(prog, sprIdx, self->nullSpriteSurface);
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

    sprite_channel_t* spr = &self->spriteChannel[sprIdx];
    if (!spr->isAcquired) {
        return EINVAL;
    }

    Surface_WritePixels(spr->surface, planes);

    return EOK;
}

errno_t _set_sprite_pos(GraphicsDriverRef _Nonnull _Locked self, int spriteId, int x, int y)
{
    const int sprIdx = GET_SPRITE_IDX(spriteId);
    if (sprIdx < 0 || sprIdx >= SPRITE_COUNT) {
        return EINVAL;
    }


    sprite_channel_t* spr = &self->spriteChannel[sprIdx];
    if (!spr->isAcquired) {
        return EINVAL;
    }

    const video_conf_t * vc = g_copper_running_prog->video_conf;
    const int16_t sprX = vc->hSprOrigin - 1 + (x >> vc->hSprScale);
    const int16_t sprY = vc->vSprOrigin + (y >> vc->vSprScale);

    spr->x = __max(__min(sprX, MAX_SPRITE_HPOS), 0);
    spr->y = __max(__min(sprY, MAX_SPRITE_VPOS), 0);

    sprite_ctl_submit(sprIdx, Surface_GetPlane(spr->surface, 0), _calc_sprite_ctl(spr));

    return EOK;
}

errno_t _set_sprite_vis(GraphicsDriverRef _Nonnull _Locked self, int spriteId, bool isVisible)
{
    decl_try_err();

    const int sprIdx = GET_SPRITE_IDX(spriteId);
    if (sprIdx < 0 || sprIdx >= SPRITE_COUNT) {
        return EINVAL;
    }

    sprite_channel_t* spr = &self->spriteChannel[sprIdx];
    if (!spr->isAcquired) {
        return EINVAL;
    }

    
    spr->isVisible = isVisible;
    copper_prog_t prog = _GraphicsDriver_GetEditableCopperProg(self);
    if (prog) {
        Surface* srf = (isVisible) ? spr->surface : self->nullSpriteSurface;

        copper_prog_sprptr_changed(prog, sprIdx, srf);
        copper_schedule(prog, 0);
    }

    return EOK;
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
