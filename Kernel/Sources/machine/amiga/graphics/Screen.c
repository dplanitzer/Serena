//
//  Screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Screen.h"
#include <machine/Platform.h>
#include <kern/kalloc.h>


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
    self->clutEntryCount = (int16_t)MAX_CLUT_ENTRIES;
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

    if (priority < 0 || priority >= NUM_HARDWARE_SPRITES) {
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
        if (sprIdx >= NUM_HARDWARE_SPRITES) {
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
    if (sprIdx >= 0 && sprIdx < NUM_HARDWARE_SPRITES) {
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
    if (sprIdx >= 0 && sprIdx < NUM_HARDWARE_SPRITES) {
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
    if (sprIdx >= 0 && sprIdx < NUM_HARDWARE_SPRITES) {
        Sprite_SetVisible(self->sprite[sprIdx], isVisible);
        return EOK;
    }
    else {
        return EINVAL;
    }
}


// Computes the size of a Copper program. The size is given in terms of the
// number of Copper instruction words.
size_t Screen_CalcCopperProgramLength(Screen* _Nonnull self)
{
    Surface* fb = self->surface;

    return self->clutEntryCount             // CLUT
            + 2 * fb->planeCount            // BPLxPT[nplanes]
            + 2                             // BPL1MOD, BPL2MOD
            + 3                             // BPLCON0, BPLCON1, BPLCON2
            + 2 * NUM_HARDWARE_SPRITES      // SPRxPT
            + 2                             // DIWSTART, DIWSTOP
            + 2                             // DDFSTART, DDFSTOP
            + 1;                            // DMACON
}

// Compiles a screen refresh Copper program into the given buffer (which must be
// big enough to store the program).
// \return a pointer to where the next instruction after the program would go 
CopperInstruction* _Nonnull Screen_MakeCopperProgram(Screen* _Nonnull self, CopperInstruction* _Nonnull ip, Sprite* _Nullable mouseCursor, bool isLightPenEnabled, bool isOddField)
{
    Surface* fb = self->surface;
    const VideoConfiguration* cfg = &self->vidConfig;
    const uint16_t w = Surface_GetWidth(fb);
    const uint16_t h = Surface_GetHeight(fb);
    const uint16_t bpr = Surface_GetBytesPerRow(fb);
    const bool isHires = VideoConfiguration_IsHires(cfg);
    const bool isLace = VideoConfiguration_IsInterlaced(cfg);
    const bool isPal = VideoConfiguration_IsPAL(cfg);
    const uint16_t ddfMod = isLace ? bpr : bpr - (w >> 3);
    const uint32_t firstLineByteOffset = isOddField ? 0 : ddfMod;
    const uint16_t lpen_bit = isLightPenEnabled ? BPLCON0F_LPEN : 0;
    

    // CLUT
    for (int i = 0, r = COLOR_BASE; i < self->clutEntryCount; i++, r += 2) {
        const CLUTEntry* ep = &self->clut[i];
        const uint16_t rgb12 = (ep->r >> 4 & 0x0f) << 8 | (ep->g >> 4 & 0x0f) << 4 | (ep->b >> 4 & 0x0f);

        *ip++ = COP_MOVE(r, rgb12);
    }


    // BPLxPT
    for (int i = 0, r = BPL_BASE; i < fb->planeCount; i++, r += 4) {
        const uint32_t bplpt = (uint32_t)(fb->plane[i]) + firstLineByteOffset;
        
        *ip++ = COP_MOVE(r + 0, (bplpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, bplpt & UINT16_MAX);
    }


    // BPLxMOD
    // Calculate the modulo:
    // - the whole scanline (visible + padding bytes) if interlace mode
    // - just the padding bytes (bytes per row - visible bytes) if non-interlace mode
    *ip++ = COP_MOVE(BPL1MOD, ddfMod);
    *ip++ = COP_MOVE(BPL2MOD, ddfMod);


    // BPLCON0
    uint16_t bplcon0 = BPLCON0F_COLOR | (uint16_t)((fb->planeCount & 0x07) << 12);

    if (isLightPenEnabled) {
        bplcon0 |= BPLCON0F_LPEN;
    }
    if (isHires) {
        bplcon0 |= BPLCON0F_HIRES;
    }
    if (isLace) {
        bplcon0 |= BPLCON0F_LACE;
    }

    *ip++ = COP_MOVE(BPLCON0, bplcon0);


    // BPLCONx
    *ip++ = COP_MOVE(BPLCON1, 0);
    *ip++ = COP_MOVE(BPLCON2, 0x0024);


    // SPRxPT
    Sprite* spr;
    uint16_t dmaf_sprite = 0;
    for (int i = 0, r = SPRITE_BASE; i < NUM_HARDWARE_SPRITES-1; i++, r += 4) {
        if (self->sprite[i]) {
            spr = self->sprite[i];
            dmaf_sprite = DMACONF_SPREN;
        }
        else {
            spr = self->nullSprite;
        }


        const uint32_t sprpt = (uint32_t)spr->data;
        *ip++ = COP_MOVE(r + 0, (sprpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, sprpt & UINT16_MAX);
    }


    // SPR7PT
    if (mouseCursor) {
        spr = mouseCursor;
        dmaf_sprite = DMACONF_SPREN;
    }
    else if (self->sprite[NUM_HARDWARE_SPRITES-1]) {
        spr = self->sprite[NUM_HARDWARE_SPRITES-1];
        dmaf_sprite = DMACONF_SPREN;
    }
    else {
        spr = self->nullSprite;
    }

    const uint32_t sprpt = (uint32_t)spr->data;
    *ip++ = COP_MOVE(SPR7PTH, (sprpt >> 16) & UINT16_MAX);
    *ip++ = COP_MOVE(SPR7PTL, sprpt & UINT16_MAX);


    // DIWSTART / DIWSTOP
    const uint16_t vStart = (isPal) ? DIW_PAL_VSTART : DIW_NTSC_VSTART;
    const uint16_t hStart = (isPal) ? DIW_PAL_HSTART : DIW_NTSC_HSTART;
    const uint16_t vStop = (isPal) ? DIW_PAL_VSTOP : DIW_NTSC_VSTOP;
    const uint16_t hStop = (isPal) ? DIW_PAL_HSTOP : DIW_NTSC_HSTOP;
    *ip++ = COP_MOVE(DIWSTART, (vStart << 8) | hStart);
    *ip++ = COP_MOVE(DIWSTOP, (vStop << 8) | hStop);


    // DDFSTART / DDFSTOP
    // DDFSTART = low res: DIWSTART / 2 - 8; high res: DIWSTART / 2 - 4
    // DDFSTOP = low res: DDFSTART + 8*(nwords - 1); high res: DDFSTART + 4*(nwords - 2)
    const uint16_t nVisibleWords = w >> 4;
    const uint16_t ddfStart = (hStart >> 1) - ((isHires) ?  4 : 8);
    const uint16_t ddfStop = ddfStart + ((isHires) ? 4*(nVisibleWords - 2) : 8*(nVisibleWords - 1));
    *ip++ = COP_MOVE(DDFSTART, ddfStart);
    *ip++ = COP_MOVE(DDFSTOP, ddfStop);


    // DMACON
    *ip++ = COP_MOVE(DMACON, DMACONF_SETCLR | DMACONF_BPLEN | dmaf_sprite | DMACONF_DMAEN);

    return ip;
}
