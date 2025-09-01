//
//  copper_comp.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/27/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "copper_comp.h"
#include <machine/amiga/chipset.h>



// Compiles a Copper program to display the null screen. The null screen shows
// nothing.
errno_t copper_comp_create_null_prog(uint16_t* _Nonnull nullSpriteData, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    copper_prog_t prog = NULL;
    const size_t instrCount = 
              1                     // DMA (OFF)
            + 1                     // CLUT
            + 3                     // BPLCON0, BPLCON1, BPLCON2
            + 2 * SPRITE_COUNT      // SPRxDATy
            + 2                     // DIWSTART, DIWSTOP
            + 2                     // DDFSTART, DDFSTOP
            + 1                     // DMACON (ON)
            + 2;                    // COP_END

    err = copper_prog_create(instrCount, &prog);
    if (err != EOK) {
        return err;
    }

    copper_instr_t* ip = prog->prog;

    // DMACON (OFF)
    *ip++ = COP_MOVE(DMACON, DMACONF_BPLEN | DMACONF_SPREN);


    // CLUT
    *ip++ = COP_MOVE(COLOR00, 0x0fff);


    // BPLCONx
    *ip++ = COP_MOVE(BPLCON0, BPLCON0F_COLOR);
    *ip++ = COP_MOVE(BPLCON1, 0);
    *ip++ = COP_MOVE(BPLCON2, 0);


    // SPRxDATy
    const uint32_t sprpt = (uint32_t)nullSpriteData;
    for (int i = 0, r = SPRITE_BASE; i < SPRITE_COUNT; i++, r += 4) {
        *ip++ = COP_MOVE(r + 0, (sprpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, sprpt & UINT16_MAX);
    }


    // DIWSTART / DIWSTOP
    *ip++ = COP_MOVE(DIWSTART, (DIW_NTSC_VSTART << 8) | DIW_NTSC_HSTART);
    *ip++ = COP_MOVE(DIWSTOP, (DIW_NTSC_VSTOP << 8) | DIW_NTSC_HSTOP);


    // DDFSTART / DDFSTOP
    *ip++ = COP_MOVE(DDFSTART, 0x0038);
    *ip++ = COP_MOVE(DDFSTOP, 0x00d0);


    // DMACON
    *ip++ = COP_MOVE(DMACON, DMACONF_SETCLR | DMACONF_SPREN | DMACONF_DMAEN);


    // end instruction
    *ip = COP_END();
    
    *pOutProg = prog;
    return EOK;
}



size_t copper_comp_calclength(const copper_params_t* _Nonnull params)
{
    return params->clut->entryCount         // CLUT
            + 2 * params->fb->planeCount    // BPLxPT[nplanes]
            + 2                             // BPL1MOD, BPL2MOD
            + 3                             // BPLCON0, BPLCON1, BPLCON2
            + 2 * SPRITE_COUNT              // SPRxPT
            + 2                             // DIWSTART, DIWSTOP
            + 2                             // DDFSTART, DDFSTOP
            + 1                             // DMACON
            + 2;                            // COP_END
}

copper_instr_t* _Nonnull copper_comp_compile(copper_instr_t* _Nonnull ip, const copper_params_t* _Nonnull params, bool isOddField)
{
    Surface* fb = params->fb;
    ColorTable* clut = params->clut;
    const uint16_t w = Surface_GetWidth(fb);
    const uint16_t h = Surface_GetHeight(fb);
    const uint16_t bpr = Surface_GetBytesPerRow(fb);
    const uint16_t ddfMod = params->isLace ? bpr : bpr - (w >> 3);
    const uint32_t firstLineByteOffset = isOddField ? 0 : ddfMod;
    const uint16_t lpen_bit = params->isLightPenEnabled ? BPLCON0F_LPEN : 0;
    

    // CLUT
    for (int i = 0, r = COLOR_BASE; i < clut->entryCount; i++, r += 2) {
        *ip++ = COP_MOVE(r, clut->entry[i]);
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

    if (params->isLightPenEnabled) {
        bplcon0 |= BPLCON0F_LPEN;
    }
    if (params->isHires) {
        bplcon0 |= BPLCON0F_HIRES;
    }
    if (params->isLace) {
        bplcon0 |= BPLCON0F_LACE;
    }

    *ip++ = COP_MOVE(BPLCON0, bplcon0);


    // BPLCONx
    *ip++ = COP_MOVE(BPLCON1, 0);
    *ip++ = COP_MOVE(BPLCON2, 0x0024);


    // SPRxPT
    for (int i = 0, r = SPRITE_BASE; i < SPRITE_COUNT; i++, r += 4) {
        const uint32_t sprpt = (uint32_t)params->sprdma[i];

        *ip++ = COP_MOVE(r + 0, (sprpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, sprpt & UINT16_MAX);
    }


    // DIWSTART / DIWSTOP
    const uint16_t vStart = (params->isPal) ? DIW_PAL_VSTART : DIW_NTSC_VSTART;
    const uint16_t hStart = (params->isPal) ? DIW_PAL_HSTART : DIW_NTSC_HSTART;
    const uint16_t vStop = (params->isPal) ? DIW_PAL_VSTOP : DIW_NTSC_VSTOP;
    const uint16_t hStop = (params->isPal) ? DIW_PAL_HSTOP : DIW_NTSC_HSTOP;
    *ip++ = COP_MOVE(DIWSTART, (vStart << 8) | hStart);
    *ip++ = COP_MOVE(DIWSTOP, (vStop << 8) | hStop);


    // DDFSTART / DDFSTOP
    // DDFSTART = low res: DIWSTART / 2 - 8; high res: DIWSTART / 2 - 4
    // DDFSTOP = low res: DDFSTART + 8*(nwords - 1); high res: DDFSTART + 4*(nwords - 2)
    const uint16_t nVisibleWords = w >> 4;
    const uint16_t ddfStart = (hStart >> 1) - ((params->isHires) ?  4 : 8);
    const uint16_t ddfStop = ddfStart + ((params->isHires) ? (nVisibleWords - 2) << 2 : (nVisibleWords - 1) << 3);
    *ip++ = COP_MOVE(DDFSTART, ddfStart);
    *ip++ = COP_MOVE(DDFSTOP, ddfStop);


    // DMACON
    *ip++ = COP_MOVE(DMACON, DMACONF_SETCLR | DMACONF_BPLEN | DMACONF_SPREN | DMACONF_DMAEN);


    // COP_END
    *ip++ = COP_END();

    return ip;
}
