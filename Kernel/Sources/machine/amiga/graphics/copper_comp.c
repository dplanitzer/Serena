//
//  copper_comp.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/27/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "copper_comp.h"
#include <machine/amiga/chipset.h>



size_t copper_comp_calclength(Screen* _Nonnull scr)
{
    Surface* fb = scr->surface;

    return scr->clutEntryCount              // CLUT
            + 2 * fb->planeCount            // BPLxPT[nplanes]
            + 2                             // BPL1MOD, BPL2MOD
            + 3                             // BPLCON0, BPLCON1, BPLCON2
            + 2 * SPRITE_COUNT              // SPRxPT
            + 2                             // DIWSTART, DIWSTOP
            + 2                             // DDFSTART, DDFSTOP
            + 1                             // DMACON
            + 1;                            // COP_END
}

copper_instr_t* _Nonnull copper_comp_compile(copper_instr_t* _Nonnull ip, Screen* _Nonnull scr, Sprite* _Nullable mouseCursor, bool isLightPenEnabled, bool isOddField)
{
    Surface* fb = scr->surface;
    const VideoConfiguration* cfg = &scr->vidConfig;
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
    for (int i = 0, r = COLOR_BASE; i < scr->clutEntryCount; i++, r += 2) {
        const CLUTEntry* ep = &scr->clut[i];
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
    for (int i = 0, r = SPRITE_BASE; i < SPRITE_COUNT-1; i++, r += 4) {
        if (scr->sprite[i]) {
            spr = scr->sprite[i];
            dmaf_sprite = DMACONF_SPREN;
        }
        else {
            spr = scr->nullSprite;
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
    else if (scr->sprite[SPRITE_COUNT-1]) {
        spr = scr->sprite[SPRITE_COUNT-1];
        dmaf_sprite = DMACONF_SPREN;
    }
    else {
        spr = scr->nullSprite;
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

    // COP_END
    *ip++ = COP_END();

    return ip;
}
