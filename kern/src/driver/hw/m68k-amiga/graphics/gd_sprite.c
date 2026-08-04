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
    const uint16_t h = _gdGetImageHeight(self->image);
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

bool _bind_sprite_image(sprite_channel_t* _Nonnull spr, image_t* _Nullable pbo)
{
    bool hasChanged = false;


    // Nothing to do if the pixel buffer doesn't actually change
    if (spr->image == pbo) {
        return false;
    }


    // Unbind the existing pixel buffer, if one is bound
    if (spr->image) {
        // Cancel any still pending control word writes
        sprite_ctl_cancel(spr->id);

        // Drop the sprite channel reference. Note that the currently running Copper
        // program still holds a reference on the sprite surface. This one will be
        // freed after the Copper program has been retired
        _gdReleaseImage(spr->image);
        spr->image = NULL;

        hasChanged = true;
    }


    // Bind the new pixel buffer if there is one
    if (pbo) {
        spr->image = pbo;
        _gdRetainImage(pbo);

        uint32_t* sprptr = (uint32_t*)_gdGetImagePlane(pbo, 0);
        *sprptr = _calc_sprite_ctl(spr);

        hasChanged = true;
    }

    return hasChanged;
}

static void _sprite_image_or_visibility_changed(const sprite_channel_t* _Nonnull spr)
{
    copper_prog_t prog = copper_get_editable_prog();
        
    if (prog) {
        copper_prog_sprptr_changed(prog, spr->id, (spr->image && spr->isVisible) ? spr->image : NULL);
        copper_schedule(prog, 0);
    }
}

static void _set_sprite_pos(sprite_channel_t* _Nonnull spr, int x, int y)
{
    spr->x = x;
    spr->y = y;

    if (spr->image) {
        const uint32_t ctl = _calc_sprite_ctl(spr);

        if (spr->isVisible) {
            sprite_ctl_submit(spr->id, _gdGetImagePlane(spr->image, 0), ctl);
        }
        else {
            uint32_t* sprptr = (uint32_t*)_gdGetImagePlane(spr->image, 0);
            *sprptr = ctl;
        }
    }
}

static sprite_channel_t* _Nullable sprite_channel_for_id(pid_t pid, int id)
{
    if (id >= 0 && id < SPRITE_COUNT) {
        sprite_channel_t* p = &g_sprite[id];

        //XXX owner check disabled for now since the console is still in the kernel.
        //XXX enable the check once the console has been moved to user space 
        //if (p->ownerPid == pid) {
            return p;
        //}
    }

    return NULL;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprite API
////////////////////////////////////////////////////////////////////////////////

errno_t _gdBindSpriteImage(pid_t pid, int spriteId, image_t* _Nullable img)
{
    sprite_channel_t* scp = sprite_channel_for_id(pid, spriteId);

    if (scp == NULL) {
        return EINVAL;
    }
    if (img) {
        if (_gdGetImageWidth(img) != SPRITE_WIDTH || _gdGetImageHeight(img) > MAX_SPRITE_HEIGHT) {
            return ENOTSUP;
        }
        if (_gdGetImagePixelFormat(img) != GD_RGB_SPRITE_2) {
            return ENOTSUP;
        }
    }
    if (spriteId == MOUSE_SPRITE_PRI && g_mouse_cursor_active) {
        return EBUSY;
    }


    if (_bind_sprite_image(scp, img)) {
        _sprite_image_or_visibility_changed(scp);
    }

    return EOK;
}

errno_t gdMoveSprite(pid_t pid, int spriteId, int x, int y)
{
    sprite_channel_t* scp = sprite_channel_for_id(pid, spriteId);

    if (scp == NULL) {
        return EINVAL;
    }
    if (spriteId == MOUSE_SPRITE_PRI && g_mouse_cursor_active) {
        return EBUSY;
    }

    _set_sprite_pos(scp, x, y);
    return EOK;
}

errno_t gdShowSprite(pid_t pid, int spriteId, bool isVisible)
{
    sprite_channel_t* scp = sprite_channel_for_id(pid, spriteId);

    if (scp == NULL) {
        return EINVAL;
    }
    if (spriteId == MOUSE_SPRITE_PRI && g_mouse_cursor_active) {
        return EBUSY;
    }


    const bool hasChange = scp->isVisible != isVisible;
    
    if (hasChange) {
        scp->isVisible = isVisible;
        _sprite_image_or_visibility_changed(scp);
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
    sprite_channel_t* scp = &g_sprite[MOUSE_SPRITE_PRI];

    g_mouse_cursor_active = 1;
    hasChanged |= _bind_sprite_image(scp, NULL);
    _set_sprite_pos(scp, 0, 0);
    hasChanged |= !scp->isVisible;
    scp->isVisible = true;

    if (hasChanged) {
        _sprite_image_or_visibility_changed(scp);
    }

    return EOK;
}

void gdReleaseCursor()
{
    if (g_mouse_cursor_active) {
        sprite_channel_t* scp = &g_sprite[MOUSE_SPRITE_PRI];
        bool hasChanged = false;
        
        hasChanged |= _bind_sprite_image(scp, NULL);
        _set_sprite_pos(scp, 0, 0);
        hasChanged |= scp->isVisible;
        scp->isVisible = false;
        g_mouse_cursor_active = 0;

        if (hasChanged) {
            _sprite_image_or_visibility_changed(scp);
        }
    }
}

errno_t _gdBindCursorImage(image_t* _Nonnull img)
{
    if (!g_mouse_cursor_active) {
        return EBUSY;
    }

    if (img) {
        if (_gdGetImageWidth(img) != HID_CURSOR_WIDTH
            || _gdGetImageHeight(img) != HID_CURSOR_HEIGHT
            || _gdGetImagePixelFormat(img) != GD_RGB_SPRITE_2) {
            return ENOTSUP;
        }
    }


    sprite_channel_t* scp = &g_sprite[MOUSE_SPRITE_PRI];

    if (_bind_sprite_image(scp, img)) {
        _sprite_image_or_visibility_changed(scp);
    }
    return EOK;
}

void gdMoveCursor(int x, int y)
{
    if (g_mouse_cursor_active) {
        _set_sprite_pos(&g_sprite[MOUSE_SPRITE_PRI], x, y);
    }
}

void gdShowCursor(bool isVisible)
{
    if (g_mouse_cursor_active) {
        sprite_channel_t* scp = &g_sprite[MOUSE_SPRITE_PRI];
        const bool hasChange = (scp->isVisible != isVisible);

        if (hasChange) {
            scp->isVisible = isVisible;
            _sprite_image_or_visibility_changed(scp);
        }
    }
}
