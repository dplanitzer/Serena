//
//  Sprite.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Sprite.h"
#include <klib/Kalloc.h>


// Creates a sprite object.
errno_t Sprite_Create(const uint16_t* _Nonnull pPlanes[2], int height, const ScreenConfiguration* _Nonnull cfg, Sprite* _Nonnull * _Nonnull pOutSelf)
{
    decl_try_err();
    const bool isPal = ScreenConfiguration_IsPal(cfg);
    Sprite* self;
    
    try(kalloc_cleared(sizeof(Sprite), (void**) &self));
    self->x = 0;
    self->y = 0;
    self->height = (uint16_t)height;
    self->diwVStart = (isPal) ? DIW_PAL_VSTART : DIW_NTSC_VSTART;
    self->diwHStart = (isPal) ? DIW_PAL_HSTART : DIW_NTSC_HSTART;
    self->shift = cfg->spr_shift;
    self->isVisible = true;


    // Construct the sprite DMA data
    const int nWords = 2 + 2*height + 2;
    try(kalloc_options(sizeof(uint16_t) * nWords, KALLOC_OPTION_UNIFIED, (void**) &self->data));
    const uint16_t* sp0 = pPlanes[0];
    const uint16_t* sp1 = pPlanes[1];
    uint16_t* dp = self->data;

    *dp++ = 0;  // sprxpos (will be filled out by the caller)
    *dp++ = 0;  // sprxctl (will be filled out by the caller)
    for (int i = 0; i < height; i++) {
        *dp++ = *sp0++;
        *dp++ = *sp1++;
    }
    *dp++ = 0;
    *dp   = 0;
    
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
static void Sprite_StateDidChange(Sprite* _Nonnull self)
{
    // Hiding a sprite means to move it all the way to X max.
    const uint16_t hshift = (self->shift & 0xf0) >> 4;
    const uint16_t vshift = self->shift & 0x0f;
    const uint16_t hstart = (self->isVisible) ? self->diwHStart - 1 + (self->x >> hshift) : 511;
    const uint16_t vstart = self->diwVStart + (self->y >> vshift);
    const uint16_t vstop = vstart + self->height;
    const uint16_t sprxpos = ((vstart & 0x00ff) << 8) | ((hstart & 0x01fe) >> 1);
    const uint16_t sprxctl = ((vstop & 0x00ff) << 8) | (((vstart >> 8) & 0x0001) << 2) | (((vstop >> 8) & 0x0001) << 1) | (hstart & 0x0001);

    self->data[0] = sprxpos;
    self->data[1] = sprxctl;
}

// Updates the position of a hardware sprite.
void Sprite_SetPosition(Sprite* _Nonnull self, int x, int y)
{
    self->x = x;
    self->y = y;
    Sprite_StateDidChange(self);
}

// Updates the visibility state of a hardware sprite.
void Sprite_SetVisible(Sprite* _Nonnull self, bool isVisible)
{
    self->isVisible = isVisible;
    Sprite_StateDidChange(self);
}
