//
//  gd_sprite.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <ext/math.h>
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

static errno_t _bind_sprite(int unit, Surface* _Nullable srf)
{
    bool doEditCopperProg = false;

    if (unit < 0 || unit >= SPRITE_COUNT) {
        return ENOTSUP;
    }
    if (srf) {
        if (Surface_GetWidth(srf) != SPRITE_WIDTH || Surface_GetHeight(srf) > MAX_SPRITE_HEIGHT) {
            return ENOTSUP;
        }
        if (Surface_GetPixelFormat(srf) != VIO_RGB_SPRITE_2) {
            return ENOTSUP;
        }
    }

    sprite_channel_t* spr = &g_sprite[unit];


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
        Surface_DelRef(spr->surface);
        spr->surface = NULL;

        doEditCopperProg = true;
    }


    // Bind the new surface if there is one
    if (srf) {
        spr->surface = srf;
        Surface_AddRef(srf);

        uint32_t* sprptr = (uint32_t*)Surface_GetPlane(srf, 0);
        *sprptr = _calc_sprite_ctl(spr);

        doEditCopperProg = true;
    }


    if (doEditCopperProg) {
        copper_prog_t prog = copper_get_editable_prog();
        
        if (prog) {
            copper_prog_sprptr_changed(prog, unit, (spr->surface && spr->isVisible) ? spr->surface : g_null_sprite_surface);
            copper_schedule(prog, 0);
        }
    }

    return EOK;
}

static errno_t _set_sprite_pos(int unit, int x, int y)
{
    if (unit < 0 || unit >= SPRITE_COUNT) {
        return EINVAL;
    }


    sprite_channel_t* spr = &g_sprite[unit];
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

static errno_t _set_sprite_vis(int unit, bool isVisible)
{
    decl_try_err();

    if (unit < 0 || unit >= SPRITE_COUNT) {
        return EINVAL;
    }

    sprite_channel_t* spr = &g_sprite[unit];
    if (spr->isVisible != isVisible) {
        spr->isVisible = isVisible;

        if (spr->surface) {
            copper_prog_t prog = copper_get_editable_prog();
        
            if (prog) {
                Surface* srf = (isVisible) ? spr->surface : g_null_sprite_surface;

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

errno_t _gdBindSprite(int unit, Surface* _Nullable srf)
{
    if (unit == MOUSE_SPRITE_PRI && g_mouse_cursor_active) {
        return EBUSY;
    }
    else {
        return _bind_sprite(unit, srf);
    }
}

errno_t gdSetSpritePos(int spriteId, int x, int y)
{
    if (spriteId == MOUSE_SPRITE_PRI && g_mouse_cursor_active) {
        return EBUSY;
    }
    else {
        return _set_sprite_pos(spriteId, x, y);
    }
}

errno_t gdSetSpriteVis(int spriteId, bool isVisible)
{
    if (spriteId == MOUSE_SPRITE_PRI && g_mouse_cursor_active) {
        return EBUSY;
    }
    else {
        return _set_sprite_vis(spriteId, isVisible);
    }
}

void gdGetSpriteCaps(vio_sprite_caps_t* _Nonnull cp)
{
    const video_conf_t* vcp = g_copper_running_prog->video_conf;

    cp->minWidth = 16;
    cp->maxWidth = 16;
    cp->minHeight = 1;
    cp->maxHeight = 256;
    cp->lowSpriteNum = (g_mouse_cursor_active) ? 1 : 0;
    cp->highSpriteNum = 7;
    cp->xScale = 1 << vcp->hSprScale;
    cp->yScale = 1 << vcp->vSprScale;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

errno_t gdObtainCursor(void)
{
    g_mouse_cursor_active = 1;
    _bind_sprite(MOUSE_SPRITE_PRI, NULL);
    _set_sprite_pos(MOUSE_SPRITE_PRI, 0, 0);
    _set_sprite_vis(MOUSE_SPRITE_PRI, true);

    return EOK;
}

void gdReleaseCursor()
{
    if (g_mouse_cursor_active) {
        _bind_sprite(MOUSE_SPRITE_PRI, NULL);
        _set_sprite_pos(MOUSE_SPRITE_PRI, 0, 0);
        _set_sprite_vis(MOUSE_SPRITE_PRI, true);
        g_mouse_cursor_active = 0;
    }
}

errno_t gdBindCursor(int id)
{
    if (g_mouse_cursor_active) {
       Surface* srf = (id != 0) ? Surface_GetForId(id) : NULL;

        if (srf || id == 0) {
            return _bind_sprite(MOUSE_SPRITE_PRI, srf);
        }
        else {
            return EINVAL;
        }
    }
    else {
        return EBUSY;
    }
}

void gdSetCursorPos(int x, int y)
{
    if (g_mouse_cursor_active) {
        _set_sprite_pos(MOUSE_SPRITE_PRI, x, y);
    }
}

void gdSetCursorVis(bool isVisible)
{
    if (g_mouse_cursor_active) {
        _set_sprite_vis(MOUSE_SPRITE_PRI, isVisible);
    }
}
