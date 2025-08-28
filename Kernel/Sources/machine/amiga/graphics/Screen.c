//
//  Screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Screen.h"
#include <kern/kalloc.h>
#include <machine/amiga/chipset.h>


// Creates a screen object.
// \param pConfig the video configuration
// \param pixelFormat the pixel format (must be supported by the config)
// \return the screen or null
errno_t Screen_Create(int id, const VideoConfiguration* _Nonnull vidCfg, Surface* _Nonnull srf, Sprite* _Nonnull pNullSprite, Screen* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    const bool isPal = VideoConfiguration_IsPAL(vidCfg);
    const bool isHires = VideoConfiguration_IsHires(vidCfg);
    const bool isLace = VideoConfiguration_IsInterlaced(vidCfg);
    Screen* self;
    
    try(kalloc_cleared(sizeof(Screen), (void**) &self));

    Surface_BeginUse(srf);
    self->id = id;
    self->surface = srf;
    self->nullSprite = pNullSprite;
    self->vidConfig = *vidCfg;
    self->clutEntryCount = (int16_t)COLOR_COUNT;
    self->flags = kScreenFlag_IsNewCopperProgNeeded;    
    self->hDiwStart = isPal ? DIW_PAL_HSTART : DIW_NTSC_HSTART;
    self->vDiwStart = isPal ? DIW_PAL_VSTART : DIW_NTSC_VSTART;
    self->hSprScale = isHires ? 0x01 : 0x00;
    self->vSprScale = isLace ? 0x01 : 0x00;
    
    if (self->clutEntryCount > 0) {
        try(kalloc_cleared(sizeof(CLUTEntry) * self->clutEntryCount, (void**)&self->clut));
    }

    *pOutSelf = self;
    return EOK;
    
catch:
    Screen_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void Screen_Destroy(Screen* _Nullable self)
{
    if (self) {
        if (self->surface) {
            Surface_EndUse(self->surface);
            self->surface = NULL;
        }
        
        if (self->clut) {
            kfree(self->clut);
            self->clut = NULL;
        }

        kfree(self);
    }
}

void Screen_GetPixelSize(Screen* _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    *pOutWidth = Surface_GetWidth(self->surface);
    *pOutHeight = Surface_GetHeight(self->surface);
}

// Writes the given RGB color to the color register at index idx
errno_t Screen_SetCLUTEntry(Screen* _Nonnull self, size_t idx, RGBColor32 color)
{
    decl_try_err();

    if (idx >= self->clutEntryCount) {
        return EINVAL;
    }

    CLUTEntry* ep = &self->clut[idx];
    ep->r = RGBColor32_GetRed(color);
    ep->g = RGBColor32_GetGreen(color);
    ep->b = RGBColor32_GetBlue(color);
    Screen_SetNeedsUpdate(self);

    return EOK;
}

// Sets the contents of 'count' consecutive CLUT entries starting at index 'idx'
// to the colors in the array 'entries'.
errno_t Screen_SetCLUTEntries(Screen* _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
{
    if (idx + count > self->clutEntryCount) {
        return EINVAL;
    }

    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            const RGBColor32 color = entries[i];
            CLUTEntry* ep = &self->clut[idx + i];

            ep->r = RGBColor32_GetRed(color);
            ep->g = RGBColor32_GetGreen(color);
            ep->b = RGBColor32_GetBlue(color);
            Screen_SetNeedsUpdate(self);
        }
    }

    return EOK;
}

errno_t Screen_AcquireSprite(Screen* _Nonnull self, int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutSpriteIdx)
{
    decl_try_err();
    Sprite* spr;

    *pOutSpriteIdx = -1;

    if (priority < 0 || priority >= SPRITE_COUNT) {
        return ENOTSUP;
    }
    if (self->sprite[priority]) {
        return EBUSY;
    }

    if ((err = Sprite_Create(width, height, pixelFormat, &spr)) == EOK) {
        self->sprite[priority] = spr;
        Screen_SetNeedsUpdate(self);
        *pOutSpriteIdx = priority;
    }

    return err;
}

// Relinquishes a hardware sprite
errno_t Screen_RelinquishSprite(Screen* _Nonnull self, int sprIdx)
{
    if (sprIdx >= 0) {
        if (sprIdx >= SPRITE_COUNT) {
            return EINVAL;
        }

        // XXX Sprite_Destroy(pScreen->sprite[spriteId]);
        // XXX actually free the old sprite instead of leaking it. Can't do this
        // XXX yet because we need to ensure that the DMA is no longer accessing
        // XXX the data before it freeing it.
        self->sprite[sprIdx] = self->nullSprite;
        Screen_SetNeedsUpdate(self);
    }
    return EOK;
}

errno_t Screen_SetSpritePixels(Screen* _Nonnull self, int sprIdx, const uint16_t* _Nonnull planes[2])
{
    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT) {
        Sprite_SetPixels(self->sprite[sprIdx], planes);
        return EOK;
    }
    else {
        return EINVAL;
    }
}

// Updates the position of a hardware sprite.
errno_t Screen_SetSpritePosition(Screen* _Nonnull self, int sprIdx, int x, int y)
{
    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT) {
        const int16_t x16 = __max(__min(x, INT16_MAX), INT16_MIN);
        const int16_t y16 = __max(__min(y, INT16_MAX), INT16_MIN);
        const int16_t sprX = self->hDiwStart - 1 + (x16 >> self->hSprScale);
        const int16_t sprY = self->vDiwStart + (y16 >> self->vSprScale);

        Sprite_SetPosition(self->sprite[sprIdx], sprX, sprY);
        return EOK;
    }
    else {
        return EINVAL;
    }
}

// Updates the visibility of a hardware sprite.
errno_t Screen_SetSpriteVisible(Screen* _Nonnull self, int sprIdx, bool isVisible)
{
    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT) {
        Sprite_SetVisible(self->sprite[sprIdx], isVisible);
        return EOK;
    }
    else {
        return EINVAL;
    }
}
