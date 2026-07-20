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


sprite_channel_t    g_sprite[SPRITE_COUNT];
uint16_t* _Nonnull  g_null_sprite_data;
static bool         g_mouse_cursor_active;


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprites
////////////////////////////////////////////////////////////////////////////////

// Called when the position or visibility of a hardware sprite has changed.
// Recalculates the sprxpos and sprxctl control words and updates them in the
// sprite DMA data block.
static uint32_t _calc_sprite_ctl(const sprite_channel_t* _Nonnull self)
{
    const uint16_t h = Surface_GetHeight(self->pixbuf);
    const video_conf_t * vc = g_cur_video_config;
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

bool _bind_sprite_buffer(sprite_channel_t* _Nonnull spr, Surface* _Nullable pbo)
{
    bool hasChanged = false;


    // Nothing to do if the pixel buffer doesn't actually change
    if (spr->pixbuf == pbo) {
        return false;
    }


    // Unbind the existing pixel buffer, if one is bound
    if (spr->pixbuf) {
        // Cancel any still pending control word writes
        sprite_ctl_cancel(spr->id);

        // Drop the sprite channel reference. Note that the currently running Copper
        // program still holds a reference on the sprite surface. This one will be
        // freed after the Copper program has been retired
        Surface_DelRef(spr->pixbuf);
        spr->pixbuf = NULL;

        hasChanged = true;
    }


    // Bind the new pixel buffer if there is one
    if (pbo) {
        spr->pixbuf = pbo;
        Surface_AddRef(pbo);

        uint32_t* sprptr = (uint32_t*)Surface_GetPlane(pbo, 0);
        *sprptr = _calc_sprite_ctl(spr);

        hasChanged = true;
    }

    return hasChanged;
}

static void _sprite_buffer_or_visibility_changed(const sprite_channel_t* _Nonnull spr)
{
    copper_prog_t prog = copper_get_editable_prog();
        
    if (prog) {
        copper_prog_sprptr_changed(prog, spr->id, (spr->pixbuf && spr->isVisible) ? spr->pixbuf : NULL);
        copper_schedule(prog, 0);
    }
}

static void _set_sprite_pos(sprite_channel_t* _Nonnull spr, int x, int y)
{
    spr->x = x;
    spr->y = y;

    if (spr->pixbuf) {
        const uint32_t ctl = _calc_sprite_ctl(spr);

        if (spr->isVisible) {
            sprite_ctl_submit(spr->id, Surface_GetPlane(spr->pixbuf, 0), ctl);
        }
        else {
            uint32_t* sprptr = (uint32_t*)Surface_GetPlane(spr->pixbuf, 0);
            *sprptr = ctl;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprite API
////////////////////////////////////////////////////////////////////////////////

errno_t _gdBindSpriteBuffer(int unit, Surface* _Nullable pbo)
{
    if (unit < 0 || unit >= SPRITE_COUNT) {
        return ENOTSUP;
    }
    if (pbo) {
        if (Surface_GetWidth(pbo) != SPRITE_WIDTH || Surface_GetHeight(pbo) > MAX_SPRITE_HEIGHT) {
            return ENOTSUP;
        }
        if (Surface_GetPixelFormat(pbo) != GD_RGB_SPRITE_2) {
            return ENOTSUP;
        }
    }
    if (unit == MOUSE_SPRITE_PRI && g_mouse_cursor_active) {
        return EBUSY;
    }


    sprite_channel_t* spr = &g_sprite[unit];
    if (_bind_sprite_buffer(spr, pbo)) {
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

void gdGetSpriteCaps(gd_sprite_caps_t* _Nonnull cp)
{
    cp->minWidth = 16;
    cp->maxWidth = 16;
    cp->minHeight = 1;
    cp->maxHeight = 256;
    cp->lowSpriteNum = (g_mouse_cursor_active) ? 1 : 0;
    cp->highSpriteNum = 7;
    cp->xScale = 1 << g_cur_video_config->hSprScale;
    cp->yScale = 1 << g_cur_video_config->vSprScale;
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
    hasChanged |= _bind_sprite_buffer(spr, NULL);
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
        
        hasChanged |= _bind_sprite_buffer(spr, NULL);
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

    Surface* pbo = (id != 0) ? Surface_GetForId(id) : NULL;
    sprite_channel_t* spr = &g_sprite[MOUSE_SPRITE_PRI];
    bool hasChanged = false;

    if (pbo) {
        if (Surface_GetWidth(pbo) != HID_CURSOR_WIDTH
            || Surface_GetHeight(pbo) != HID_CURSOR_HEIGHT
            || Surface_GetPixelFormat(pbo) != GD_RGB_SPRITE_2) {
            return ENOTSUP;
        }

        hasChanged = _bind_sprite_buffer(spr, pbo);
    }
    else if (id == 0) {
        hasChanged = _bind_sprite_buffer(spr, NULL);
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
