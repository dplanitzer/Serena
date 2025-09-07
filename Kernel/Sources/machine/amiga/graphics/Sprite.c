//
//  Sprite.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Sprite.h"
#include <kern/kalloc.h>
#include <kern/kernlib.h>

static void _Sprite_StateDidChange(Sprite* _Nonnull self);


// Initializes the given sprite as a null sprite. Meaning that it doesn't show
// anything and that it isn't acquired.
void Sprite_Init(Sprite* _Nonnull self)
{
    self->data = NULL;
    self->x = 0;
    self->y = 0;
    self->height = 0;
    self->isAcquired = false;
}

// Acquires a sprite of size 'width' x 'height' pixels and initial position (x, y).
// The sprite pixels are set to transparent by default. You must call
// Sprite_SetPixels() with non-transparent pixels before anything will show up
// on the screen. 
errno_t Sprite_Acquire(Sprite* _Nonnull self, int x, int y, int width, int height, PixelFormat pixelFormat)
{
    decl_try_err();

    if (width != SPRITE_WIDTH) {
        return EINVAL;
    }
    if (height < 0 || height > MAX_SPRITE_HEIGHT) {
        return EINVAL;
    }
    if (pixelFormat != kPixelFormat_RGB_Indexed2) {
        return ENOTSUP;
    }

    self->x = __max(__min(x, MAX_SPRITE_HPOS), 0);
    self->y = __max(__min(y, MAX_SPRITE_VPOS), 0);
    self->height = (uint16_t)height;

    const size_t byteCount = (2 + 2*height + 2) * sizeof(uint16_t);
    try(kalloc_options(byteCount, KALLOC_OPTION_UNIFIED | KALLOC_OPTION_CLEAR, (void**)&self->data));

    _Sprite_StateDidChange(self);
    self->isAcquired = true;

catch:
    return err;
}

void Sprite_Relinquish(Sprite* _Nonnull self)
{
    kfree(self->data);
    self->data = NULL;

    self->isAcquired = false;
    self->x = 0;
    self->y = 0;
    self->height = 0;
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

    if (ye > MAX_SPRITE_VPOS || ye < y) {
        ye = MAX_SPRITE_VPOS;
        y = ye - self->height;
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
}

// Updates the position of a hardware sprite.
void Sprite_SetPosition(Sprite* _Nonnull self, int16_t x, int16_t y)
{
    self->x = __max(__min(x, MAX_SPRITE_HPOS), 0);
    self->y = __max(__min(y, MAX_SPRITE_VPOS), 0);
    _Sprite_StateDidChange(self);
}
