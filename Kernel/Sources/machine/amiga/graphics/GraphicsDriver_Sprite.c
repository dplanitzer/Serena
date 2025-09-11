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

// Called when the position or visibility of a hardware sprite has changed.
// Recalculates the sprxpos and sprxctl control words and updates them in the
// sprite DMA data block.
static uint32_t _calc_sprite_ctl(const sprite_channel_t* _Nonnull self)
{
    const uint16_t h = Surface_GetHeight(self->surface);
    const video_conf_t * vc = g_copper_running_prog->video_conf;
    const int16_t sprX = vc->hSprOrigin - 1 + (self->x >> vc->hSprScale);
    const int16_t sprY = vc->vSprOrigin + (self->y >> vc->vSprScale);
    uint16_t x = __max(__min(sprX, MAX_SPRITE_HPOS), 0);
    uint16_t y = __max(__min(sprY, MAX_SPRITE_VPOS), 0);
    uint16_t ye = y + h;

    if (ye > MAX_SPRITE_VPOS || ye < y) {
        ye = MAX_SPRITE_VPOS;
        y = ye - h;
    }

    const uint32_t hw = ((y & 0x00ff) << 8) | ((x & 0x01fe) >> 1);
    const uint32_t lw = ((ye & 0x00ff) << 8) | (((y >> 8) & 0x0001) << 2) | (((ye >> 8) & 0x0001) << 1) | (x & 0x0001);

    return (hw << 16) | lw;
}

static errno_t _bind_sprite(GraphicsDriverRef _Nonnull _Locked self, int unit, Surface* _Nullable srf)
{
    bool doEditCopperProg = false;

    if (unit < 0 || unit >= SPRITE_COUNT) {
        return ENOTSUP;
    }
    if (srf) {
        if (Surface_GetWidth(srf) != SPRITE_WIDTH || Surface_GetHeight(srf) > MAX_SPRITE_HEIGHT) {
            return ENOTSUP;
        }
        if (Surface_GetPixelFormat(srf) != kPixelFormat_RGB_Sprite2) {
            return ENOTSUP;
        }
    }

    sprite_channel_t* spr = &self->spriteChannel[unit];


    // Nothing to do if the surface doesn't actually change
    if (spr->surface == srf) {
        return EOK;
    }


    // Unbind the existing surface, if one is bound
    if (spr->surface) {
        // Cancel any still pending control word writes
        sprite_ctl_cancel(unit);

        // Drop the sprite channel reference. Note that the currently running Copper
        // program still holds a reference on the sprite surface. This one will be
        // freed after the Copper program has been retired
        GObject_DelRef(spr->surface);
        spr->surface = NULL;

        doEditCopperProg = true;
    }


    // Bind the new surface if there is one
    if (srf) {
        spr->surface = srf;
        GObject_AddRef(srf);

        uint32_t* sprptr = (uint32_t*)Surface_GetPlane(srf, 0);
        *sprptr = _calc_sprite_ctl(spr);

        doEditCopperProg = true;
    }


    if (doEditCopperProg) {
        copper_prog_t prog = _GraphicsDriver_GetEditableCopperProg(self);
        
        if (prog) {
            copper_prog_sprptr_changed(prog, unit, (spr->surface && spr->isVisible) ? spr->surface : self->nullSpriteSurface);
            copper_schedule(prog, 0);
        }
    }

    return EOK;
}

static errno_t _set_sprite_pixels(GraphicsDriverRef _Nonnull _Locked self, int unit, const uint16_t* _Nonnull planes[2])
{
    if (unit < 0 || unit >= SPRITE_COUNT) {
        return EINVAL;
    }

    sprite_channel_t* spr = &self->spriteChannel[unit];
    Surface_WritePixels(spr->surface, (const void**)planes, 2, kPixelFormat_RGB_Indexed2);
    return EOK;
}

static errno_t _set_sprite_pos(GraphicsDriverRef _Nonnull _Locked self, int unit, int x, int y)
{
    if (unit < 0 || unit >= SPRITE_COUNT) {
        return EINVAL;
    }


    sprite_channel_t* spr = &self->spriteChannel[unit];
    spr->x = x;
    spr->y = y;
    if (spr->surface) {
        const uint32_t ctl = _calc_sprite_ctl(spr);

        if (spr->isVisible) {
            sprite_ctl_submit(unit, Surface_GetPlane(spr->surface, 0), ctl);
        }
        else {
            uint32_t* sprptr = (uint32_t*)Surface_GetPlane(spr->surface, 0);
            *sprptr = ctl;
        }
    }
    return EOK;
}

static errno_t _set_sprite_vis(GraphicsDriverRef _Nonnull _Locked self, int unit, bool isVisible)
{
    decl_try_err();

    if (unit < 0 || unit >= SPRITE_COUNT) {
        return EINVAL;
    }

    sprite_channel_t* spr = &self->spriteChannel[unit];
    if (spr->isVisible != isVisible) {
        spr->isVisible = isVisible;

        if (spr->surface) {
            copper_prog_t prog = _GraphicsDriver_GetEditableCopperProg(self);
        
            if (prog) {
                Surface* srf = (isVisible) ? spr->surface : self->nullSpriteSurface;

                copper_prog_sprptr_changed(prog, unit, srf);
                copper_schedule(prog, 0);
            }
        }
    }

    return EOK;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprite API
////////////////////////////////////////////////////////////////////////////////

errno_t _GraphicsDriver_BindSprite(GraphicsDriverRef _Nonnull _Locked self, int unit, Surface* _Nullable srf)
{
    if (unit == MOUSE_SPRITE_PRI && self->flags.isMouseCursorObtained) {
        return EBUSY;
    }
    else {
        return _bind_sprite(self, unit, srf);
    }
}

errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, int spriteId, int x, int y)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    if (spriteId == MOUSE_SPRITE_PRI && self->flags.isMouseCursorObtained) {
        err = EBUSY;
    }
    else {
        err = _set_sprite_pos(self, spriteId, x, y);
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, int spriteId, bool isVisible)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    if (spriteId == MOUSE_SPRITE_PRI && self->flags.isMouseCursorObtained) {
        err = EBUSY;
    }
    else {
        err = _set_sprite_vis(self, spriteId, isVisible);
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

void GraphicsDriver_GetSpriteCaps(GraphicsDriverRef _Nonnull self, SpriteCaps* _Nonnull cp)
{
    mtx_lock(&self->io_mtx);
    const video_conf_t* vcp = g_copper_running_prog->video_conf;

    cp->minWidth = 16;
    cp->maxWidth = 16;
    cp->minHeight = 1;
    cp->maxHeight = 256;
    cp->lowSpriteNum = (self->flags.isMouseCursorObtained) ? 1 : 0;
    cp->highSpriteNum = 7;
    cp->xScale = 1 << vcp->hSprScale;
    cp->yScale = 1 << vcp->vSprScale;
    mtx_unlock(&self->io_mtx);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_ObtainMouseCursor(GraphicsDriverRef _Nonnull self)
{
    mtx_lock(&self->io_mtx);
    self->flags.isMouseCursorObtained = 1;
    _bind_sprite(self, MOUSE_SPRITE_PRI, NULL);
    _set_sprite_pos(self, MOUSE_SPRITE_PRI, 0, 0);
    _set_sprite_vis(self, MOUSE_SPRITE_PRI, true);
    mtx_unlock(&self->io_mtx);
    return EOK;
}

void GraphicsDriver_ReleaseMouseCursor(GraphicsDriverRef _Nonnull self)
{
    mtx_lock(&self->io_mtx);
    if (self->flags.isMouseCursorObtained) {
        _bind_sprite(self, MOUSE_SPRITE_PRI, NULL);
        _set_sprite_pos(self, MOUSE_SPRITE_PRI, 0, 0);
        _set_sprite_vis(self, MOUSE_SPRITE_PRI, true);
        self->flags.isMouseCursorObtained = 0;
    }
    mtx_unlock(&self->io_mtx);
}

errno_t GraphicsDriver_BindMouseCursor(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    if (self->flags.isMouseCursorObtained) {
       Surface* srf = (id != 0) ? _GraphicsDriver_GetSurfaceForId(self, id) : NULL;
        if (srf || id == 0) {
            err = _bind_sprite(self, MOUSE_SPRITE_PRI, srf);
        }
        else {
            err = EINVAL;
        }
    }
    else {
        err = EBUSY;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, int x, int y)
{
    mtx_lock(&self->io_mtx);
    if (self->flags.isMouseCursorObtained) {
        _set_sprite_pos(self, MOUSE_SPRITE_PRI, x, y);
    }
    mtx_unlock(&self->io_mtx);
}

void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull self, bool isVisible)
{
    mtx_lock(&self->io_mtx);
    if (self->flags.isMouseCursorObtained) {
        _set_sprite_vis(self, MOUSE_SPRITE_PRI, isVisible);
    }
    mtx_unlock(&self->io_mtx);
}
