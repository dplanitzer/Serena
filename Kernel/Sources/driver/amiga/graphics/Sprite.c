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
errno_t Sprite_Create(const uint16_t* _Nonnull pPlanes[2], int height, const VideoConfiguration* _Nonnull cfg, Sprite* _Nonnull * _Nonnull pOutSelf)
{
    decl_try_err();
    const bool isPal = VideoConfiguration_IsPAL(cfg);
    const bool isHires = VideoConfiguration_IsHires(cfg);
    const bool isLace = VideoConfiguration_IsInterlaced(cfg);
    Sprite* self;
    
    try(kalloc_cleared(sizeof(Sprite), (void**) &self));
    self->x = 0;
    self->y = 0;
    self->height = (uint16_t)height;
    self->hDiwStart = isPal ? DIW_PAL_HSTART : DIW_NTSC_HSTART;
    self->vDiwStart = isPal ? DIW_PAL_VSTART : DIW_NTSC_VSTART;
    self->hShift = isHires ? 0x01 : 0x00;
    self->vShift = isLace ? 0x01 : 0x00;
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
    const uint16_t hstart = (self->isVisible) ? self->hDiwStart - 1 + (self->x >> self->hShift) : 511;
    const uint16_t vstart = self->vDiwStart + (self->y >> self->vShift);
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
