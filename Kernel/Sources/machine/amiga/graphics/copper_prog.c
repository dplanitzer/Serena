//
//  copper_prog.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/27/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "copper.h"
#include "ColorTable.h"
#include <kern/kalloc.h>


errno_t copper_prog_create(size_t instr_count, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    copper_prog_t prog = NULL;

    if (instr_count == 0) {
        return EINVAL;
    }


    // Allocate a new program if we aren't able to reuse a retired program
    err = kalloc_cleared(sizeof(struct copper_prog), (void**)&prog);
    if (err != EOK) {
        return err;
    }

    err = kalloc_options(sizeof(copper_instr_t) * instr_count, KALLOC_OPTION_UNIFIED, (void**)&prog->prog);
    if (err != EOK) {
        kfree(prog);
        return err;
    }

    
    // Prepare the program state
    prog->prog_size = instr_count;
    prog->state = COP_STATE_IDLE;
    prog->odd_entry = prog->prog;
    prog->even_entry = NULL;


    *pOutProg = prog;
    return EOK;
}

void copper_prog_destroy(copper_prog_t _Nullable prog)
{
    if (prog) {
        kfree(prog->prog);
        prog->prog = NULL;
        prog->even_entry = NULL;
        prog->odd_entry = NULL;

        GObject_DelRef(prog->res.fb);
        prog->res.fb = NULL;
        GObject_DelRef(prog->res.clut);
        prog->res.clut = NULL;

        for (int i = 0; i < SPRITE_COUNT; i++) {
            GObject_DelRef(prog->res.spr[i]);
            prog->res.spr[i] = NULL;
        }
    }
    kfree(prog);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Compilation
////////////////////////////////////////////////////////////////////////////////

size_t calc_copper_prog_instruction_count(const video_conf_t* _Nonnull vc)
{
    const int isLace = (vc->flags & VCFLAG_LACE) != 0;
    size_t len = COLOR_COUNT                // CLUT
            + 1                             // SPRxPTR/BPLxPTR DMA Barrier
            + 2 * SPRITE_COUNT              // SPRxPT
            + 2 * PLANE_COUNT               // BPLxPT[nplanes]
            + 2                             // BPL1MOD, BPL2MOD
            + 3                             // BPLCON0, BPLCON1, BPLCON2
            + 2                             // DIWSTART, DIWSTOP
            + 2                             // DDFSTART, DDFSTOP
            + 1                             // DMACON
            + 1;                            // COP_END

    return (isLace) ? 2 * len : len;
}

static copper_instr_t* _Nonnull _compile_field_prog(
    copper_instr_t* _Nonnull ip, 
    copper_locs_t* _Nullable locs,
    const video_conf_t* _Nonnull vc,
    Surface* _Nullable fb,
    ColorTable* _Nonnull clut,
    const sprite_channel_t _Nonnull spr[],
    Surface* _Nonnull nullSpriteSurface,
    bool isLightPenEnabled,
    bool isOddField)
{
    const int isHires = (vc->flags & VCFLAG_HIRES) != 0;
    const int isLace = (vc->flags & VCFLAG_LACE) != 0;
    const uint16_t w = vc->width;
    const uint16_t h = vc->height;
    copper_instr_t* orig = ip;

    assert(clut->entryCount == COLOR_COUNT);
    assert((fb && fb->planeCount < PLANE_COUNT) || (fb == NULL));


    // CLUT
    if (locs) {
        locs->clut = ip - orig;
    }
    for (int i = 0, r = COLOR_BASE; i < COLOR_COUNT; i++, r += 2) {
        *ip++ = COP_MOVE(r, clut->entry[i]);
    }


    // SPRxPTR/BPLxPTR DMA Barrier
    // We wait here so that the Copper program editing code gets more time to
    // change sprite pointers before this program pokes them into the DMA
    // registers. 
    *ip++ = COP_WAIT(20, 0, 0xff, 0);


    // SPRxPT
    if (locs) {
        locs->sprptr = ip - orig;
    }
    for (int i = 0, r = SPRITE_BASE; i < SPRITE_COUNT; i++, r += 4) {
        Surface* srf = (spr[i].isAcquired && spr[i].isVisible) ? spr[i].surface : nullSpriteSurface;
        const uint32_t sprpt = (uint32_t)Surface_GetPlane(srf, 0);

        *ip++ = COP_MOVE(r + 0, (sprpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, sprpt & UINT16_MAX);
    }


    // BPLxPT
    if (fb) {
        const uint16_t bpr = Surface_GetBytesPerRow(fb);
        const uint16_t ddfMod = (isLace) ? bpr : bpr - (w >> 3);
        const uint32_t firstLineByteOffset = isOddField ? 0 : ddfMod;

        for (int i = 0, r = BPL_BASE; i < fb->planeCount; i++, r += 4) {
            const uint32_t bplpt = (uint32_t)fb->plane[i] + firstLineByteOffset;
        
            *ip++ = COP_MOVE(r + 0, (bplpt >> 16) & UINT16_MAX);
            *ip++ = COP_MOVE(r + 2, bplpt & UINT16_MAX);
        }


        // BPLxMOD
        // Calculate the modulo:
        // - the whole scanline (visible + padding bytes) if interlace mode
        // - just the padding bytes (bytes per row - visible bytes) if non-interlace mode
        *ip++ = COP_MOVE(BPL1MOD, ddfMod);
        *ip++ = COP_MOVE(BPL2MOD, ddfMod);
    }


    // BPLCON0
    const uint16_t bp_cnt = (fb) ? fb->planeCount & 0x07 : 0;
    const uint16_t lpen_bit = (isLightPenEnabled) ? BPLCON0F_LPEN : 0;
    uint16_t bplcon0 = BPLCON0F_COLOR | lpen_bit | (bp_cnt << 12);

    if (isHires) {
        bplcon0 |= BPLCON0F_HIRES;
    }
    if (isLace) {
        bplcon0 |= BPLCON0F_LACE;
    }

    if (locs) {
        locs->bplcon0 = ip - orig;
    }
    *ip++ = COP_MOVE(BPLCON0, bplcon0);


    // BPLCONx
    *ip++ = COP_MOVE(BPLCON1, 0);
    *ip++ = COP_MOVE(BPLCON2, 0x0024);


    // DIWSTART / DIWSTOP
    *ip++ = COP_MOVE(DIWSTART, ((uint16_t)vc->vDwStart << 8) | vc->hDwStart);
    *ip++ = COP_MOVE(DIWSTOP, ((uint16_t)vc->vDwStop << 8) | vc->hDwStop);


    // DDFSTART / DDFSTOP
    // DDFSTART = low res:  DIWSTART / 2 - 8
    //            high res: DIWSTART / 2 - 4
    // DDFSTOP  = low res:  DDFSTART + 8*(nwords - 1)
    //            high res: DDFSTART + 4*(nwords - 2)
    const uint16_t nVisibleWords = w >> 4;
    const uint16_t ddfStart = (vc->hDwStart >> 1) - ((isHires) ?  4 : 8);
    const uint16_t ddfStop = ddfStart + ((isHires) ? (nVisibleWords - 2) << 2 : (nVisibleWords - 1) << 3);
    *ip++ = COP_MOVE(DDFSTART, ddfStart);
    *ip++ = COP_MOVE(DDFSTOP, ddfStop);


    // DMACON
    const uint16_t bpl_bit = (fb) ? DMACONF_BPLEN : 0;
    *ip++ = COP_MOVE(DMACON, DMACONF_SETCLR | bpl_bit | DMACONF_SPREN | DMACONF_DMAEN);


    // COP_END
    *ip++ = COP_END();

    return ip;
}

void copper_prog_compile(copper_prog_t _Nonnull self, const video_conf_t* _Nonnull vc, Surface* _Nullable fb, ColorTable* _Nonnull clut, const sprite_channel_t _Nonnull spr[], Surface* _Nonnull nullSpriteSurface, bool isLightPenEnabled)
{
    const int isLace = (vc->flags & VCFLAG_LACE) != 0;
    copper_instr_t* ip;
    
    self->odd_entry = self->prog;
    self->even_entry = NULL;
    
    ip = self->prog;
    ip = _compile_field_prog(ip, &self->loc, vc, fb, clut, spr, nullSpriteSurface, isLightPenEnabled, true);

    if (isLace) {
        self->even_entry = ip;
        ip = _compile_field_prog(ip, NULL, vc, fb, clut, spr, nullSpriteSurface, isLightPenEnabled, false);
    }

    self->video_conf = vc;

    self->res.fb = fb;
    if (fb) GObject_AddRef(self->res.fb);
    self->res.clut = clut;
    GObject_AddRef(self->res.clut);

    for (int i = 0; i < SPRITE_COUNT; i++) {
        Surface* srf = (spr[i].isAcquired && spr[i].isVisible) ? spr[i].surface : nullSpriteSurface;

        self->res.spr[i] = srf;
        GObject_AddRef(srf);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Editing
////////////////////////////////////////////////////////////////////////////////

void copper_prog_set_lp_enabled(copper_prog_t _Nonnull self, bool isEnabled)
{
    if (isEnabled) {
        self->odd_entry[self->loc.bplcon0] |= BPLCON0F_LPEN;
        if (self->even_entry) {
            self->even_entry[self->loc.bplcon0] |= BPLCON0F_LPEN;
        }
    }
    else {
        self->odd_entry[self->loc.bplcon0] &= ~BPLCON0F_LPEN;
        if (self->even_entry) {
            self->even_entry[self->loc.bplcon0] &= ~BPLCON0F_LPEN;
        }
    }
}

void copper_prog_clut_changed(copper_prog_t _Nonnull self, size_t startIdx, size_t count)
{
    const uint16_t l = startIdx;
    const uint16_t h = startIdx + count;
    ColorTable* clut = (ColorTable*)self->res.clut;
    copper_instr_t* op = &self->odd_entry[self->loc.clut + l];
    copper_instr_t* ep = (self->even_entry) ? &self->even_entry[self->loc.clut + l] : NULL;
    
    for (uint16_t i = l, r = COLOR_BASE + (l << 1); i < h; i++, r += 2) {
        *op++ = COP_MOVE(r, clut->entry[i]);
        if (ep) {
            *ep++ = COP_MOVE(r, clut->entry[i]);
        }
    }
}

void copper_prog_sprptr_changed(copper_prog_t _Nonnull self, int spridx, Surface* _Nonnull srf)
{
    uint32_t sprptr = (uint32_t)Surface_GetPlane(srf, 0);
    copper_instr_t* op = &self->odd_entry[self->loc.sprptr + (spridx << 1)];
    copper_instr_t* ep = (self->even_entry) ? &self->even_entry[self->loc.sprptr + (spridx << 1)] : NULL;
    const uint16_t r = SPRITE_BASE + (spridx << 2);
    const uint16_t lp = sprptr & UINT16_MAX;
    const uint16_t hp = sprptr >> 16;

    op[0] = COP_MOVE(r + 0, hp);
    op[1] = COP_MOVE(r + 2, lp);

    if (ep) {
        ep[0] = COP_MOVE(r + 0, hp);
        ep[1] = COP_MOVE(r + 2, lp);
    }

    if (self->res.spr[spridx] != srf) {
        GObject_AddRef(srf);
        GObject_DelRef(self->res.spr[spridx]);
        self->res.spr[spridx] = srf;
    }
}
