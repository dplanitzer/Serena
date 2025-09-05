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
#include <machine/irq.h>


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
            + 2 * PLANE_COUNT               // BPLxPT[nplanes]
            + 2                             // BPL1MOD, BPL2MOD
            + 3                             // BPLCON0, BPLCON1, BPLCON2
            + 2 * SPRITE_COUNT              // SPRxPT
            + 2                             // DIWSTART, DIWSTOP
            + 2                             // DDFSTART, DDFSTOP
            + 1                             // DMACON
            + 1;                            // COP_END

    return (isLace) ? 2 * len : len;
}

static copper_instr_t* _Nonnull _compile_field_prog(copper_instr_t* _Nonnull ip, copper_locs_t* _Nullable locs, const video_conf_t* _Nonnull vc, Surface* _Nullable fb, ColorTable* _Nonnull clut, uint16_t* _Nonnull sprdma[], bool isLightPenEnabled, bool isOddField)
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


    // SPRxPT
    if (locs) {
        locs->sprptr = ip - orig;
    }
    for (int i = 0, r = SPRITE_BASE; i < SPRITE_COUNT; i++, r += 4) {
        const uint32_t sprpt = (uint32_t)sprdma[i];

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

void copper_prog_compile(copper_prog_t _Nonnull self, const video_conf_t* _Nonnull vc, Surface* _Nullable fb, ColorTable* _Nonnull clut, uint16_t* _Nonnull sprdma[], bool isLightPenEnabled)
{
    const int isLace = (vc->flags & VCFLAG_LACE) != 0;
    copper_instr_t* ip;
    
    self->odd_entry = self->prog;
    self->even_entry = NULL;
    
    ip = self->prog;
    ip = _compile_field_prog(ip, &self->loc, vc, fb, clut, sprdma, isLightPenEnabled, true);

    if (isLace) {
        self->even_entry = ip;
        ip = _compile_field_prog(ip, NULL, vc, fb, clut, sprdma, isLightPenEnabled, false);
    }

    self->video_conf = vc;
    self->res.fb = (GObject*)fb;
    self->res.clut = (GObject*)clut;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Editing
////////////////////////////////////////////////////////////////////////////////

uint8_t     g_pending_edits;
uint16_t    g_clut_low_idx;             // Index of lowest CLU entry that has changed
uint16_t    g_clut_high_idx;            // Index of highest CLUT entry that has changed plus one
uint32_t    g_sprptr[SPRITE_COUNT+1];   // 31..8: sprite dma pointer; 7..0: sprite number (0xff -> marks end of list)


void copper_cur_set_lp_enabled(bool isEnabled)
{
    // We directly poke the Copper instructions because this setting doesn't
    // depend on the BPL or SPR DMA and it has no impact on the display. So
    // whatever temporary glitching this may cause won't be visible. Do the
    // update with VBL masked to ensure that the program doesn't get retired
    // while we're changing it.
    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    copper_prog_t prog = g_copper_running_prog;
    if (isEnabled) {
        prog->odd_entry[prog->loc.bplcon0] |= BPLCON0F_LPEN;
        if (prog->even_entry) {
            prog->even_entry[prog->loc.bplcon0] |= BPLCON0F_LPEN;
        }
    }
    else {
        prog->odd_entry[prog->loc.bplcon0] &= ~BPLCON0F_LPEN;
        if (prog->even_entry) {
            prog->even_entry[prog->loc.bplcon0] &= ~BPLCON0F_LPEN;
        }
    }
    irq_set_mask(sim);
}

void copper_cur_set_sprptr(int spridx, uint16_t* _Nonnull sprptr)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    uint32_t* sp = g_sprptr;
    uint8_t sp_idx;

    for (;;) {
        sp_idx = (*sp) & 0xff;

        if (sp_idx == 0xff) {
            *sp = ((uint32_t)sprptr) << 8 | (uint8_t)spridx;
            *(sp + 1) = COPED_SPRPTR_SENTINEL;
            break;
        }
        
        if (sp_idx == (uint8_t)spridx) {
            *sp = ((uint32_t)sprptr) << 8 | sp_idx;
            break;
        }

        sp++;
    }
    g_pending_edits |= COPED_SPRPTR;
    irq_set_mask(sim);
}

void copper_cur_set_clut_range(size_t idx, size_t count)
{
    int16_t l = __max(__min(idx, COLOR_COUNT-1), 0);
    int16_t h = __max(__min(l + count, COLOR_COUNT-1), 0);

    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    g_clut_low_idx = __min(g_clut_low_idx, l);
    g_clut_high_idx = __max(g_clut_high_idx, h);
    g_pending_edits |= COPED_CLUT;
    irq_set_mask(sim);
}


void copper_cur_clear_edits(void)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    copper_clear_edits_irq();
    irq_set_mask(sim);
}

void copper_prog_apply_edits(copper_prog_t _Nonnull self, copper_instr_t* ep)
{
    if ((g_pending_edits & COPED_CLUT) != 0) {
        const uint16_t l = g_clut_low_idx;
        const uint16_t h = g_clut_high_idx;
        copper_instr_t* ip = &ep[self->loc.clut + l];
        ColorTable* clut = (ColorTable*)self->res.clut;

        for (uint16_t i = l, r = COLOR_BASE + (l << 1); i < h; i++, r += 2) {
            *ip++ = COP_MOVE(r, clut->entry[i]);
        }
    }

    if ((g_pending_edits & COPED_SPRPTR) != 0) {
        copper_instr_t* ip = &ep[self->loc.sprptr];
        const uint32_t* sp = g_sprptr;

        for (;;) {
            const uint8_t spr_idx = (*sp) & 0xff;
            const uint32_t spr_ptr = (*sp) >> 8;

            if (spr_idx == 0xff) {
                break;
            }

            ip[(spr_idx << 1) + 0] = COP_MOVE(SPRITE_BASE + (spr_idx << 2) + 0, (spr_ptr >> 16) & UINT16_MAX);
            ip[(spr_idx << 1) + 1] = COP_MOVE(SPRITE_BASE + (spr_idx << 2) + 2, spr_ptr & UINT16_MAX);
            sp++;
        }
    }
}
