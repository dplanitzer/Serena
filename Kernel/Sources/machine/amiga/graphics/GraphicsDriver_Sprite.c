//
//  GraphicsDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"
#include "copper.h"
#include <kpi/hid.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprites
////////////////////////////////////////////////////////////////////////////////

#define MAKE_SPRITE_ID(__sprIdx) \
((__sprIdx) + 1)

#define GET_SPRITE_IDX(__sprId) \
((__sprId) - 1)


// Acquires a hardware sprite
errno_t _acquire_sprite(GraphicsDriverRef _Nonnull _Locked self, int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutSpriteId)
{
    decl_try_err();

    if (priority < 0 || priority >= SPRITE_COUNT) {
        return ENOTSUP;
    }

    if (self->spriteDmaPtr[priority] != self->nullSpriteData) {
        throw(EBUSY);
    }

    // Set the initial sprite position to the top-left corner of the current
    // display window.
    const video_conf_t* vc = g_copper_running_prog->video_conf;
    const int16_t sprX = vc->hSprOrigin - 1;
    const int16_t sprY = vc->vSprOrigin;

    try(Sprite_Acquire(&self->sprite[priority], sprX, sprY, width, height, pixelFormat));
    self->spriteDmaPtr[priority] = self->sprite[priority].data;
    copper_cur_set_sprptr(priority, self->spriteDmaPtr[priority]);

catch:
    *pOutSpriteId = (err == EOK) ? MAKE_SPRITE_ID(priority) : 0;
    return err;
}

// Relinquishes a hardware sprite
errno_t _relinquish_sprite(GraphicsDriverRef _Nonnull _Locked self, int spriteId)
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    if (sprIdx < 0) {
        return EOK;
    }
    if (sprIdx >= SPRITE_COUNT) {
        return EINVAL;
    }

    if (sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        // XXX Sprite_Destroy(pScreen->sprite[spriteId]);
        // XXX actually free the old sprite instead of leaking it. Can't do this
        // XXX yet because we need to ensure that the DMA is no longer accessing
        // XXX the data before it freeing it.
        self->spriteDmaPtr[sprIdx] = self->nullSpriteData;
        copper_cur_set_sprptr(sprIdx, self->nullSpriteData);
    }
    else {
        err = EINVAL;
    }
    return err;
}

errno_t _set_sprite_pixels(GraphicsDriverRef _Nonnull _Locked self, int spriteId, const uint16_t* _Nonnull planes[2])
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        Sprite_SetPixels(&self->sprite[sprIdx], planes);
    }
    else {
        err = EINVAL;
    }
    return err;
}

// Updates the position of a hardware sprite.
errno_t _set_sprite_pos(GraphicsDriverRef _Nonnull _Locked self, int spriteId, int x, int y)
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        const video_conf_t * vc = g_copper_running_prog->video_conf;
        const int16_t sprX = vc->hSprOrigin - 1 + (x >> vc->hSprScale);
        const int16_t sprY = vc->vSprOrigin + (y >> vc->vSprScale);

        Sprite_SetPosition(&self->sprite[sprIdx], sprX, sprY);
    }
    else {
        err = EINVAL;
    }
    return err;
}

// Updates the visibility of a hardware sprite.
errno_t _set_sprite_vis(GraphicsDriverRef _Nonnull _Locked self, int spriteId, bool isVisible)
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        self->spriteDmaPtr[sprIdx] = (isVisible) ? self->sprite[sprIdx].data : self->nullSpriteData;
        copper_cur_set_sprptr(sprIdx, self->spriteDmaPtr[sprIdx]);
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
