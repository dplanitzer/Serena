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


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprites
////////////////////////////////////////////////////////////////////////////////

// Called when the position or visibility of a hardware sprite has changed.
// Recalculates the sprxpos and sprxctl control words and updates them in the
// sprite DMA data block.
uint32_t _calc_sprite_ctl(const sprite_channel_t* _Nonnull self)
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

void _sprite_image_or_visibility_changed(const sprite_channel_t* _Nonnull spr)
{
    copper_prog_t prog = copper_get_editable_prog();
        
    if (prog) {
        copper_prog_sprptr_changed(prog, spr->id, (spr->image && spr->isVisible) ? spr->image : NULL);
        copper_schedule(prog, 0);
    }
}

static sprite_channel_t* _Nullable _sprite_channel_for_id(pid_t pid, int id)
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

errno_t gdAcquireSprites(pid_t pid, int basePriority, size_t count, int* _Nonnull pOutSpriteIds)
{
    size_t alloced_count = 0;

    if (count == 0) {
        return EOK;
    }

    if (basePriority >= 0) {
        for (int i = basePriority; i < (basePriority + __min(SPRITE_COUNT, count)) && (g_sprite[i].ownerPid == 0); i++) {
            g_sprite[i].ownerPid = pid;
            pOutSpriteIds[alloced_count++] = i;
        }
    }
    else {
        for (int i = 0; i < __min(SPRITE_COUNT, count); i++) {
            if (g_sprite[i].ownerPid == 0) {
                g_sprite[i].ownerPid = pid;
                pOutSpriteIds[alloced_count++] = i;
            }
        }
    }


    if (alloced_count < count) {
        // Couldn't get everything - undo what we did
        for (size_t i = 0; i < alloced_count; i++) {
            g_sprite[pOutSpriteIds[i]].ownerPid = 0;
        }

        return EAGAIN;
    }
    else {
        return EOK;
    }
}

static void _release_sprite(sprite_channel_t* _Nonnull scp)
{
    bool hasChanged = false;
        
    hasChanged |= _bind_sprite_image(scp, NULL);
    hasChanged |= !scp->isVisible;
    scp->isVisible = true;
    scp->x = 0;
    scp->y = 0;
    scp->ownerPid = 0;

    if (hasChanged) {
        _sprite_image_or_visibility_changed(scp);
    }
}

errno_t gdReleaseSprites(pid_t pid, const int* _Nullable spriteIds, size_t count)
{
    if (spriteIds) {
        for (size_t i = 0; i < count; i++) {
            sprite_channel_t* scp = _sprite_channel_for_id(pid, spriteIds[i]);

            if (scp) {
                _release_sprite(scp);
            }
        }
    }
    else {
        for (size_t i = 0; i < SPRITE_COUNT; i++) {
            sprite_channel_t* scp = _sprite_channel_for_id(pid, i);

            if (scp) {
                _release_sprite(scp);
            }
        }
    }

    return EOK;
}

errno_t _gdBindSpriteImage(pid_t pid, int spriteId, image_t* _Nullable img)
{
    sprite_channel_t* scp = _sprite_channel_for_id(pid, spriteId);

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


    if (_bind_sprite_image(scp, img)) {
        _sprite_image_or_visibility_changed(scp);
    }

    return EOK;
}

errno_t gdBindSpriteImage(pid_t pid, int spriteId, int imageId)
{
    image_t* img = (imageId != 0) ? _gdGetImageById(pid, imageId) : NULL;
    
    if (img || imageId == 0) {
        return _gdBindSpriteImage(pid, spriteId, img);
    }
    else {
        return EINVAL;
    }
}

errno_t gdMoveSprite(pid_t pid, int spriteId, int16_t x, int16_t y)
{
    sprite_channel_t* scp = _sprite_channel_for_id(pid, spriteId);

    if (scp == NULL) {
        return EINVAL;
    }

    scp->x = x;
    scp->y = y;

    if (scp->image) {
        const uint32_t ctl = _calc_sprite_ctl(scp);
        uint32_t* ctl_ptr = (uint32_t*)_gdGetImagePlane(scp->image, 0);

        if (scp->isVisible) {
            sprite_ctl_submit(scp->id, ctl_ptr, ctl);
        }
        else {
            *ctl_ptr = ctl;
        }
    }
    return EOK;
}

errno_t gdShowSprite(pid_t pid, int spriteId, bool isVisible)
{
    sprite_channel_t* scp = _sprite_channel_for_id(pid, spriteId);

    if (scp == NULL) {
        return EINVAL;
    }

    if (scp->isVisible != isVisible) {
        scp->isVisible = isVisible;

        if (scp->image) {
            _sprite_image_or_visibility_changed(scp);
        }
    }

    return EOK;
}

void gdGetSpriteCaps(gd_sprite_constraints_t* _Nonnull cp)
{
    cp->minWidth = 16;
    cp->maxWidth = 16;
    cp->minHeight = 1;
    cp->maxHeight = 256;
    cp->maxPriority = 7;
    cp->xShift = g_cur_video_config->hSprScale;
    cp->yShift = g_cur_video_config->vSprScale;
}
