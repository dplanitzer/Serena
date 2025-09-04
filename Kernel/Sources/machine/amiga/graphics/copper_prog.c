//
//  copper_prog.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/27/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "copper.h"
#include <kern/kalloc.h>
#include <machine/amiga/chipset.h>


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

static copper_instr_t* _Nonnull _compile_field_prog(copper_instr_t* _Nonnull ip, const video_conf_t* _Nonnull vc, Surface* _Nullable fb, ColorTable* _Nonnull clut, uint16_t* _Nonnull sprdma[], bool isLightPenEnabled, bool isOddField)
{
    const int isHires = (vc->flags & VCFLAG_HIRES) != 0;
    const int isLace = (vc->flags & VCFLAG_LACE) != 0;
    const uint16_t w = vc->width;
    const uint16_t h = vc->height;

    assert(clut->entryCount == COLOR_COUNT);
    assert((fb && fb->planeCount < PLANE_COUNT) || (fb == NULL));


    // CLUT
    for (int i = 0, r = COLOR_BASE; i < COLOR_COUNT; i++, r += 2) {
        *ip++ = COP_MOVE(r, clut->entry[i]);
    }


    // SPRxPT
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
    ip = _compile_field_prog(ip, vc, fb, clut, sprdma, isLightPenEnabled, true);

    if (isLace) {
        self->even_entry = ip;
        ip = _compile_field_prog(ip, vc, fb, clut, sprdma, isLightPenEnabled, false);
    }

    self->video_conf = vc;
    self->res.fb = (GObject*)fb;
    self->res.clut = (GObject*)clut;
}
