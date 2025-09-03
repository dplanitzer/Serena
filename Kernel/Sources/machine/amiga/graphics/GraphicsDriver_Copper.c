//
//  GraphicsDriver_Copper.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/27/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"


static errno_t _create_copper_prog(GraphicsDriverRef _Nonnull _Locked self, size_t instr_count, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    copper_prog_t prog = NULL;
    copper_prog_t cp = self->copperProgCache;
    copper_prog_t pp = NULL;

    // Find a retired program that is big enough to hold 'instr_count' instructions 
    while (cp) {
        if (cp->prog_size >= instr_count) {
            if (pp) {
                pp->next = cp->next;
            }
            else {
                self->copperProgCache = cp->next;
            }

            prog = cp;
            prog->next = NULL;
            self->copperProgCacheCount--;
            break;
        }
        pp = cp;
        cp = cp->next;
    }


    // Allocate a new program if we aren't able to reuse a retired program
    if (prog == NULL) {
        err = copper_prog_create(instr_count, &prog);
        if (err != EOK) {
            return err;
        }
    }


    // Prepare the program state
    prog->state = COP_STATE_IDLE;
    prog->odd_entry = prog->prog;
    prog->even_entry = NULL;


    *pOutProg = prog;
    return EOK;
}

static void _cache_copper_prog(GraphicsDriverRef _Nonnull _Locked self, copper_prog_t _Nonnull prog)
{
    ColorTable* clut = (ColorTable*)prog->res.clut;
    Surface* fb = (Surface*)prog->res.fb;

    if (clut) {
        if (GObject_DelUse(clut)) {
            _GraphicsDriver_DestroyGObj(self, clut);
        }
        prog->res.clut = NULL;
    }
    if (fb) {
        if (GObject_DelUse(fb)) {
            _GraphicsDriver_DestroyGObj(self, fb);
        }
        prog->res.fb = NULL;
    }


    if (self->copperProgCacheCount >= MAX_CACHED_COPPER_PROGS) {
        copper_prog_destroy(prog);
        return;
    }

    if (self->copperProgCache) {
        prog->next = self->copperProgCache;
        self->copperProgCache = prog;
    }
    else {
        prog->next = NULL;
        self->copperProgCache = prog;
    }
    self->copperProgCacheCount++;
}


void GraphicsDriver_CopperManager(GraphicsDriverRef _Nonnull self)
{
    mtx_lock(&self->io_mtx);

    for (;;) {
        copper_prog_t prog;
        int signo;
        bool hasChange = false;

        while ((prog = copper_acquire_retired_prog()) != NULL) {
            _cache_copper_prog(self, prog);
            hasChange = true;
        }


        if (hasChange && self->screenConfigObserver) {
            vcpu_sigsend_irq(self->screenConfigObserver, self->screenConfigObserverSignal, false);
        }


        mtx_unlock(&self->io_mtx);
        vcpu_sigwait(&self->copvpWaitQueue, &self->copvpSigs, &signo);
        mtx_lock(&self->io_mtx);
    }

    mtx_unlock(&self->io_mtx);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Null Program
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_CreateNullCopperProg(GraphicsDriverRef _Nonnull _Locked self, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    copper_prog_t prog = NULL;
    const video_conf_t* vc = get_null_video_conf();
    const size_t instrCount = 
              1                     // DMA (OFF)
            + 1                     // CLUT
            + 3                     // BPLCON0, BPLCON1, BPLCON2
            + 2 * SPRITE_COUNT      // SPRxDATy
            + 2                     // DIWSTART, DIWSTOP
            + 2                     // DDFSTART, DDFSTOP
            + 1                     // DMACON (ON)
            + 1;                    // COP_END

    err = _create_copper_prog(self, instrCount, &prog);
    if (err != EOK) {
        return err;
    }

    copper_instr_t* ip = prog->prog;

    // DMACON (OFF)
    *ip++ = COP_MOVE(DMACON, DMACONF_BPLEN | DMACONF_SPREN);


    // CLUT
    *ip++ = COP_MOVE(COLOR00, 0x0fff);


    // SPRxDATy
    const uint32_t sprpt = (uint32_t)self->nullSpriteData;
    for (int i = 0, r = SPRITE_BASE; i < SPRITE_COUNT; i++, r += 4) {
        *ip++ = COP_MOVE(r + 0, (sprpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, sprpt & UINT16_MAX);
    }


    // BPLCONx
    *ip++ = COP_MOVE(BPLCON0, BPLCON0F_COLOR);
    *ip++ = COP_MOVE(BPLCON1, 0);
    *ip++ = COP_MOVE(BPLCON2, 0);


    // DIWSTART / DIWSTOP
    *ip++ = COP_MOVE(DIWSTART, (((uint16_t)vc->vDwStart) << 8) | vc->hDwStart);
    *ip++ = COP_MOVE(DIWSTOP, (((uint16_t)vc->vDwStop) << 8) | vc->hDwStop);


    // DDFSTART / DDFSTOP
    *ip++ = COP_MOVE(DDFSTART, 0x0038);
    *ip++ = COP_MOVE(DDFSTOP, 0x00d0);


    // DMACON
    *ip++ = COP_MOVE(DMACON, DMACONF_SETCLR | DMACONF_SPREN | DMACONF_DMAEN);


    // end instruction
    *ip = COP_END();
    

    prog->video_conf = vc;
    prog->res.fb = NULL;
    prog->res.clut = NULL;

    *pOutProg = prog;
    return EOK;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Screen Program
////////////////////////////////////////////////////////////////////////////////


static size_t _calc_copper_prog_len(Surface* _Nonnull fb)
{
    return COLOR_COUNT                      // CLUT
            + 2 * fb->planeCount            // BPLxPT[nplanes]
            + 2                             // BPL1MOD, BPL2MOD
            + 3                             // BPLCON0, BPLCON1, BPLCON2
            + 2 * SPRITE_COUNT              // SPRxPT
            + 2                             // DIWSTART, DIWSTOP
            + 2                             // DDFSTART, DDFSTOP
            + 1                             // DMACON
            + 1;                            // COP_END
}

static copper_instr_t* _Nonnull _compile_copper_prog(GraphicsDriverRef _Nonnull self, copper_instr_t* _Nonnull ip, const video_conf_t* _Nonnull vc, Surface* _Nonnull fb, ColorTable* _Nonnull clut, bool isOddField)
{
    const int isHires = (vc->flags & VCFLAG_HIRES) != 0;
    const int isLace = (vc->flags & VCFLAG_LACE) != 0;
    const uint16_t w = vc->width;
    const uint16_t h = vc->height;
    const uint16_t bpr = Surface_GetBytesPerRow(fb);
    const uint16_t ddfMod = (isLace) ? bpr : bpr - (w >> 3);
    const uint32_t firstLineByteOffset = isOddField ? 0 : ddfMod;
    const uint16_t lpen_bit = self->flags.isLightPenEnabled ? BPLCON0F_LPEN : 0;
    
    assert(clut->entryCount == COLOR_COUNT);


    // CLUT
    for (int i = 0, r = COLOR_BASE; i < COLOR_COUNT; i++, r += 2) {
        *ip++ = COP_MOVE(r, clut->entry[i]);
    }


    // SPRxPT
    for (int i = 0, r = SPRITE_BASE; i < SPRITE_COUNT; i++, r += 4) {
        const uint32_t sprpt = (uint32_t)self->spriteDmaPtr[i];

        *ip++ = COP_MOVE(r + 0, (sprpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, sprpt & UINT16_MAX);
    }


    // BPLxPT
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


    // BPLCON0
    uint16_t bplcon0 = BPLCON0F_COLOR | lpen_bit | (uint16_t)((fb->planeCount & 0x07) << 12);

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
    *ip++ = COP_MOVE(DMACON, DMACONF_SETCLR | DMACONF_BPLEN | DMACONF_SPREN | DMACONF_DMAEN);


    // COP_END
    *ip++ = COP_END();

    return ip;
}

errno_t GraphicsDriver_CreateScreenCopperProg(GraphicsDriverRef _Nonnull _Locked self, const video_conf_t* _Nonnull vc, Surface* _Nonnull fb, ColorTable* _Nonnull clut, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    const int isLace = (vc->flags & VCFLAG_LACE) != 0;
    copper_prog_t prog = NULL;
    copper_instr_t* ip;
    
    const size_t instrCount = _calc_copper_prog_len(fb);

    try(_create_copper_prog(self, instrCount, &prog));
    prog->odd_entry = prog->prog;
    prog->even_entry = NULL;
    
    ip = prog->prog;
    ip = _compile_copper_prog(self, ip, vc, fb, clut, true);

    if (isLace) {
        prog->even_entry = ip;
        ip = _compile_copper_prog(self, ip, vc, fb, clut, false);
    }

    prog->video_conf = vc;
    prog->res.fb = (GObject*)fb;
    prog->res.clut = (GObject*)clut;

    GObject_AddUse(fb);
    GObject_AddUse(clut);

catch:
    *pOutProg = prog;

    return err;
}
