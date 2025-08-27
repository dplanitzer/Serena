//
//  Sprite.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Sprite.h"
#include <kern/kalloc.h>


// Creates a sprite of size 'width' x 'height' pixels and with position (0, 0).
// Pixels must be assigned separately by calling Sprite_SetPixels() before anything
// will show up on the screen. 
errno_t Sprite_Create(int width, int height, PixelFormat pixelFormat, Sprite* _Nonnull * _Nonnull pOutSelf)
{
    decl_try_err();
    Sprite* self;

    if (width != SPRITE_WIDTH) {
        throw(EINVAL);
    }
    if (height < 0 || height > MAX_SPRITE_HEIGHT) {
        throw(EINVAL);
    }
    if (pixelFormat != kPixelFormat_RGB_Indexed2) {
        throw(ENOTSUP);
    }

    try(kalloc_cleared(sizeof(Sprite), (void**) &self));
    self->isVisible = true;
    self->height = (uint16_t)height;

    const size_t byteCount = (2 + 2*height + 2) * sizeof(uint16_t);
    try(kalloc_options(byteCount, KALLOC_OPTION_UNIFIED, (void**)&self->data));

    // No pixel data yet - configure the sprite as a null sprite
    self->data[0] = 0;  // sprxpos (will be filled out when user assigns a position)
    self->data[1] = 0;  // sprxctl (will be filled out when user assigns a position)

    *pOutSelf = self;
    return EOK;

catch:
    Sprite_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void Sprite_Destroy(Sprite* _Nullable self)
{
    if (self) {
        kfree(self->data);
        self->data = NULL;
        
        kfree(self);
    }
}

// Called when the position or visibility of a hardware sprite has changed.
// Recalculates the sprxpos and sprxctl control words and updates them in the
// sprite DMA data block.
static void _Sprite_StateDidChange(Sprite* _Nonnull self)
{
    // Hiding a sprite means to move it all the way to X max.
    int x = self->x;
    int y = self->y;
    int ye = y + self->height;

    if (y < 0 || ye > MAX_SPRITE_VPOS || ye < y) {
        ye = MAX_SPRITE_VPOS;
        y = ye - self->height;
    }

    if (!self->isVisible || x < 0 || x > MAX_SPRITE_HPOS) {
        x = MAX_SPRITE_HPOS;
    }

    self->data[0] = ((y & 0x00ff) << 8) | ((x & 0x01fe) >> 1);
    self->data[1] = ((ye & 0x00ff) << 8) | (((y >> 8) & 0x0001) << 2) | (((ye >> 8) & 0x0001) << 1) | (x & 0x0001);
}

void Sprite_SetPixels(Sprite* _Nonnull self, const uint16_t* _Nonnull planes[2])
{
    const uint16_t* sp0 = planes[0];
    const uint16_t* sp1 = planes[1];
    uint16_t* dp = &self->data[2];

    for (uint16_t i = 0; i < self->height; i++) {
        *dp++ = *sp0++;
        *dp++ = *sp1++;
    }
    *dp++ = 0;
    *dp   = 0;

    // A sprite starts out as a null sprite. Now that pixels have been assigned,
    // make sure that the sprite will show up on the screen
    _Sprite_StateDidChange(self);
}

// Updates the position of a hardware sprite. All of 'x' < 0, 'y' < 0,
// 'x' > MAX_SPRITE_HPOS and 'y' > MAX_SPRITE_VPOS - sprite_height hide the
// sprite.
void Sprite_SetPosition(Sprite* _Nonnull self, int16_t x, int16_t y)
{
    self->x = x;
    self->y = y;
    _Sprite_StateDidChange(self);
}

// Updates the visibility state of a hardware sprite.
void Sprite_SetVisible(Sprite* _Nonnull self, bool isVisible)
{
    if (self->isVisible != isVisible) {
        uint16_t sprxpos = self->data[0];
        uint16_t sprxctl = self->data[1];

        // Hiding a sprite means to move it all the way to X max.
        if (isVisible) {
            int x = self->x;
            
            if (x < 0 || x > MAX_SPRITE_HPOS) {
                // hide
                x = MAX_SPRITE_HPOS;
            }

            sprxpos &= 0xff00;
            sprxctl &= 0xfffe;
            sprxpos |= (x & 0x01fe) >> 1;
            sprxctl |= x & 0x0001;
        }
        else {
            sprxpos |= 0x00ff;
            sprxctl |= 0x0001;
        }

        self->data[0] = sprxpos;
        self->data[1] = sprxctl;
        self->isVisible = isVisible;
    }
}
