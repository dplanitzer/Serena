//
//  GraphicsDriver_Copper.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/27/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Copper Management
////////////////////////////////////////////////////////////////////////////////

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
    copper_prog_clear_edits(prog);


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
// MARK: Copper Program Generators
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_CreateNullCopperProg(GraphicsDriverRef _Nonnull _Locked self, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    ColorTable* clut;

    err = _GraphicsDriver_CreateCLUT(self, COLOR_COUNT, kRGBColor32_White, &clut);
    if (err != EOK) {
        return err;
    }

    err = GraphicsDriver_CreateScreenCopperProg(self, get_null_video_conf(), NULL, clut, pOutProg);
    if (err != EOK) {
        _GraphicsDriver_DestroyGObj(self, clut);
        return err;
    }

    return EOK;
}

errno_t GraphicsDriver_CreateScreenCopperProg(GraphicsDriverRef _Nonnull _Locked self, const video_conf_t* _Nonnull vc, Surface* _Nullable fb, ColorTable* _Nonnull clut, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    const size_t instrCount = calc_copper_prog_instruction_count(vc);
    copper_prog_t prog = NULL;

    err = _create_copper_prog(self, instrCount, &prog);
    if (err == EOK) {
        copper_prog_compile(prog, vc, fb, clut, self->spriteDmaPtr, self->flags.isLightPenEnabled);
        
        if (fb) GObject_AddUse(fb);
        GObject_AddUse(clut);
    }

    *pOutProg = prog;

    return err;
}
