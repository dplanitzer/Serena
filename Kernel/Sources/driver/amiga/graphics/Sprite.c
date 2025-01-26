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
errno_t Sprite_Create(const uint16_t* _Nonnull pPlanes[2], int height, Sprite* _Nonnull * _Nonnull pOutSprite)
{
    decl_try_err();
    Sprite* pSprite;
    
    try(kalloc_cleared(sizeof(Sprite), (void**) &pSprite));
    pSprite->x = 0;
    pSprite->y = 0;
    pSprite->height = (uint16_t)height;
    pSprite->isVisible = true;


    // Construct the sprite DMA data
    const int nWords = 2 + 2*height + 2;
    try(kalloc_options(sizeof(uint16_t) * nWords, KALLOC_OPTION_UNIFIED, (void**) &pSprite->data));
    const uint16_t* sp0 = pPlanes[0];
    const uint16_t* sp1 = pPlanes[1];
    uint16_t* dp = pSprite->data;

    *dp++ = 0;  // sprxpos (will be filled out by the caller)
    *dp++ = 0;  // sprxctl (will be filled out by the caller)
    for (int i = 0; i < height; i++) {
        *dp++ = *sp0++;
        *dp++ = *sp1++;
    }
    *dp++ = 0;
    *dp   = 0;
    
    *pOutSprite = pSprite;
    return EOK;
    
catch:
    Sprite_Destroy(pSprite);
    *pOutSprite = NULL;
    return err;
}

void Sprite_Destroy(Sprite* _Nullable pSprite)
{
    if (pSprite) {
        kfree(pSprite->data);
        pSprite->data = NULL;
        
        kfree(pSprite);
    }
}

// Called when the position or visibility of a hardware sprite has changed.
// Recalculates the sprxpos and sprxctl control words and updates them in the
// sprite DMA data block.
static void Sprite_StateDidChange(Sprite* _Nonnull pSprite, const ScreenConfiguration* pConfig)
{
    // Hiding a sprite means to move it all the way to X max.
    const uint16_t hshift = (pConfig->spr_shift & 0xf0) >> 4;
    const uint16_t vshift = pConfig->spr_shift & 0x0f;
    const uint16_t hstart = (pSprite->isVisible) ? pConfig->diw_start_h - 1 + (pSprite->x >> hshift) : 511;
    const uint16_t vstart = pConfig->diw_start_v + (pSprite->y >> vshift);
    const uint16_t vstop = vstart + pSprite->height;
    const uint16_t sprxpos = ((vstart & 0x00ff) << 8) | ((hstart & 0x01fe) >> 1);
    const uint16_t sprxctl = ((vstop & 0x00ff) << 8) | (((vstart >> 8) & 0x0001) << 2) | (((vstop >> 8) & 0x0001) << 1) | (hstart & 0x0001);

    pSprite->data[0] = sprxpos;
    pSprite->data[1] = sprxctl;
}

// Updates the position of a hardware sprite.
void Sprite_SetPosition(Sprite* _Nonnull pSprite, int x, int y, const ScreenConfiguration* pConfig)
{
    pSprite->x = x;
    pSprite->y = y;
    Sprite_StateDidChange(pSprite, pConfig);
}

// Updates the visibility state of a hardware sprite.
void Sprite_SetVisible(Sprite* _Nonnull pSprite, bool isVisible, const ScreenConfiguration* pConfig)
{
    pSprite->isVisible = isVisible;
    Sprite_StateDidChange(pSprite, pConfig);
}
