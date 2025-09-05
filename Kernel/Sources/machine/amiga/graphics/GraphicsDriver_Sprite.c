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
errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutSpriteId)
{
    decl_try_err();

    if (priority < 0 || priority >= SPRITE_COUNT) {
        return ENOTSUP;
    }

    mtx_lock(&self->io_mtx);
    if (self->spriteDmaPtr[priority] != self->nullSpriteData) {
        throw(EBUSY);
    }

    try(Sprite_Acquire(&self->sprite[priority], width, height, pixelFormat));
    self->spriteDmaPtr[priority] = self->sprite[priority].data;
    copper_cur_set_sprptr(priority, self->spriteDmaPtr[priority]);

catch:
    mtx_unlock(&self->io_mtx);
    *pOutSpriteId = (err == EOK) ? MAKE_SPRITE_ID(priority) : 0;
    return err;
}

// Relinquishes a hardware sprite
errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull self, int spriteId)
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    if (sprIdx < 0) {
        return EOK;
    }
    if (sprIdx >= SPRITE_COUNT) {
        return EINVAL;
    }

    mtx_lock(&self->io_mtx);
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
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_SetSpritePixels(GraphicsDriverRef _Nonnull self, int spriteId, const uint16_t* _Nonnull planes[2])
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    mtx_lock(&self->io_mtx);
    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        Sprite_SetPixels(&self->sprite[sprIdx], planes);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

// Updates the position of a hardware sprite.
errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, int spriteId, int x, int y)
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    mtx_lock(&self->io_mtx);
    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        const video_conf_t * vc = g_copper_running_prog->video_conf;
        const int16_t sprX = vc->hSprOrigin - 1 + (x >> vc->hSprScale);
        const int16_t sprY = vc->vSprOrigin + (y >> vc->vSprScale);

        Sprite_SetPosition(&self->sprite[sprIdx], sprX, sprY);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

// Updates the visibility of a hardware sprite.
errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, int spriteId, bool isVisible)
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    mtx_lock(&self->io_mtx);
    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        self->spriteDmaPtr[sprIdx] = (isVisible) ? self->sprite[sprIdx].data : self->nullSpriteData;
        copper_cur_set_sprptr(sprIdx, self->spriteDmaPtr[sprIdx]);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull self, const uint16_t* _Nullable planes[2], int width, int height, PixelFormat pixelFormat)
{
    if ((width > 0 && height > 0) && (width != kCursor_Width || height != kCursor_Height || pixelFormat != kCursor_PixelFormat)) {
        return ENOTSUP;
    }

    mtx_lock(&self->io_mtx);
    const unsigned oldMouseCursorEnabled = self->flags.mouseCursorEnabled;

    if (width > 0 && height > 0) {
        Sprite_SetPixels(&self->mouseCursor, planes);
        if (self->mouseCursor.x == 0 && self->mouseCursor.y == 0) {
            // Make sure that the initial mouse cursor position is the top-left
            // corner of the display window. Don't change the position if the
            // HID manager has already assigned a position to the mouse cursor.
            const video_conf_t* vc = g_copper_running_prog->video_conf;
            const int16_t sprX = vc->hSprOrigin - 1;
            const int16_t sprY = vc->vSprOrigin;

            Sprite_SetPosition(&self->mouseCursor, sprX, sprY);
        }
        self->flags.mouseCursorEnabled = 1;
    }
    else {
        self->flags.mouseCursorEnabled = 0;
    }

    if (oldMouseCursorEnabled != self->flags.mouseCursorEnabled) {
        uint16_t* sprptr = (self->flags.mouseCursorEnabled) ? self->mouseCursor.data : self->nullSpriteData;;

        self->spriteDmaPtr[MOUSE_SPRITE_PRI] = sprptr;
        copper_cur_set_sprptr(MOUSE_SPRITE_PRI, sprptr);
    }
    mtx_unlock(&self->io_mtx);
    return EOK;
}

void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, int x, int y)
{
    mtx_lock(&self->io_mtx);
    const video_conf_t* vc = g_copper_running_prog->video_conf;
    const int16_t sprX = vc->hSprOrigin - 1 + (x >> vc->hSprScale);
    const int16_t sprY = vc->vSprOrigin + (y >> vc->vSprScale);

    Sprite_SetPosition(&self->mouseCursor, sprX, sprY);
    mtx_unlock(&self->io_mtx);
}

void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull self, bool isVisible)
{
    mtx_lock(&self->io_mtx);
    if (self->flags.mouseCursorEnabled) {
        self->spriteDmaPtr[MOUSE_SPRITE_PRI] = (isVisible) ? self->mouseCursor.data : self->nullSpriteData;
        copper_cur_set_sprptr(MOUSE_SPRITE_PRI, self->spriteDmaPtr[MOUSE_SPRITE_PRI]);
    }
    mtx_unlock(&self->io_mtx);
}
