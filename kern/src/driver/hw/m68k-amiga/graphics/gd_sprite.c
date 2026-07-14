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

static bool _bind_sprite(sprite_channel_t* _Nonnull spr, Surface* _Nullable srf)
{
    bool hasChanged = false;


    // Nothing to do if the surface doesn't actually change
    if (spr->surface == srf) {
        return false;
    }


    // Unbind the existing surface, if one is bound
    if (spr->surface) {
        // Cancel any still pending control word writes
        sprite_ctl_cancel(spr->id);

        // Drop the sprite channel reference. Note that the currently running Copper
        // program still holds a reference on the sprite surface. This one will be
        // freed after the Copper program has been retired
        Surface_DelRef(spr->surface);
        spr->surface = NULL;

        hasChanged = true;
    }


    // Bind the new surface if there is one
    if (srf) {
        spr->surface = srf;
        Surface_AddRef(srf);

        uint32_t* sprptr = (uint32_t*)Surface_GetPlane(srf, 0);
        *sprptr = _calc_sprite_ctl(spr);

        hasChanged = true;
    }

    return hasChanged;
}

static void _sprite_buffer_or_visibility_changed(const sprite_channel_t* _Nonnull spr)
{
    copper_prog_t prog = copper_get_editable_prog();
        
    if (prog) {
        copper_prog_sprptr_changed(prog, spr->id, (spr->surface && spr->isVisible) ? spr->surface : g_null_sprite_surface);
        copper_schedule(prog, 0);
    }
}

static void _set_sprite_pos(sprite_channel_t* _Nonnull spr, int x, int y)
{
    spr->x = x;
    spr->y = y;

    if (spr->surface) {
        const uint32_t ctl = _calc_sprite_ctl(spr);

        if (spr->isVisible) {
            sprite_ctl_submit(spr->id, Surface_GetPlane(spr->surface, 0), ctl);
        }
        else {
            uint32_t* sprptr = (uint32_t*)Surface_GetPlane(spr->surface, 0);
            *sprptr = ctl;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprite API
////////////////////////////////////////////////////////////////////////////////

errno_t _gdBindSprite(int unit, Surface* _Nullable srf)
{
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
    if (unit == MOUSE_SPRITE_PRI && g_mouse_cursor_active) {
        return EBUSY;
    }


    sprite_channel_t* spr = &g_sprite[unit];
    if (_bind_sprite(spr, srf)) {
        _sprite_buffer_or_visibility_changed(spr);
    }

    return EOK;
}

errno_t gdSetSpritePos(int spriteId, int x, int y)
{
    if (spriteId < 0 || spriteId >= SPRITE_COUNT) {
        return EINVAL;
    }
    if (spriteId == MOUSE_SPRITE_PRI && g_mouse_cursor_active) {
        return EBUSY;
    }

    _set_sprite_pos(&g_sprite[spriteId], x, y);
    return EOK;
}

errno_t gdSetSpriteVis(int spriteId, bool isVisible)
{
    if (spriteId < 0 || spriteId >= SPRITE_COUNT) {
        return EINVAL;
    }
    if (spriteId == MOUSE_SPRITE_PRI && g_mouse_cursor_active) {
        return EBUSY;
    }


    sprite_channel_t* spr = &g_sprite[spriteId];
    const bool hasChange = spr->isVisible != isVisible;
    
    if (hasChange) {
        spr->isVisible = isVisible;
        _sprite_buffer_or_visibility_changed(spr);
    }

    return EOK;
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
    bool hasChanged = false;
    sprite_channel_t* spr = &g_sprite[MOUSE_SPRITE_PRI];

    g_mouse_cursor_active = 1;
    hasChanged |= _bind_sprite(spr, NULL);
    _set_sprite_pos(spr, 0, 0);
    hasChanged |= !spr->isVisible;
    spr->isVisible = true;

    if (hasChanged) {
        _sprite_buffer_or_visibility_changed(spr);
    }

    return EOK;
}

void gdReleaseCursor()
{
    if (g_mouse_cursor_active) {
        sprite_channel_t* spr = &g_sprite[MOUSE_SPRITE_PRI];
        bool hasChanged = false;
        
        hasChanged |= _bind_sprite(spr, NULL);
        _set_sprite_pos(spr, 0, 0);
        hasChanged |= spr->isVisible;
        spr->isVisible = false;
        g_mouse_cursor_active = 0;

        if (hasChanged) {
            _sprite_buffer_or_visibility_changed(spr);
        }
    }
}

errno_t gdBindCursor(int id)
{
    if (!g_mouse_cursor_active) {
        return EBUSY;
    }

    Surface* srf = (id != 0) ? Surface_GetForId(id) : NULL;
    sprite_channel_t* spr = &g_sprite[MOUSE_SPRITE_PRI];
    bool hasChanged = false;

    if (srf) {
        if (Surface_GetWidth(srf) != HID_CURSOR_WIDTH
            || Surface_GetHeight(srf) != HID_CURSOR_HEIGHT
            || Surface_GetPixelFormat(srf) != VIO_RGB_SPRITE_2) {
            return ENOTSUP;
        }

        hasChanged = _bind_sprite(spr, srf);
    }
    else if (id == 0) {
        hasChanged = _bind_sprite(spr, NULL);
    }
    else {
        return EINVAL;
    }

    if (hasChanged) {
        _sprite_buffer_or_visibility_changed(spr);
    }
    return EOK;
}

void gdSetCursorPos(int x, int y)
{
    if (g_mouse_cursor_active) {
        _set_sprite_pos(&g_sprite[MOUSE_SPRITE_PRI], x, y);
    }
}

void gdSetCursorVis(bool isVisible)
{
    if (g_mouse_cursor_active) {
        sprite_channel_t* spr = &g_sprite[MOUSE_SPRITE_PRI];
        const bool hasChange = (spr->isVisible != isVisible);

        if (hasChange) {
            spr->isVisible = isVisible;
            _sprite_buffer_or_visibility_changed(spr);
        }
    }
}
