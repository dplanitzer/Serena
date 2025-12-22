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


    *pOutProg = prog;
    return EOK;
}

static void _cache_copper_prog(GraphicsDriverRef _Nonnull _Locked self, copper_prog_t _Nonnull prog)
{
    ColorTable* clut = (ColorTable*)prog->res.clut;
    Surface* fb = (Surface*)prog->res.fb;

    GObject_DelRef(clut);
    prog->res.clut = NULL;
    GObject_DelRef(fb);
    prog->res.fb = NULL;

    for (int i = 0; i < SPRITE_COUNT; i++) {
        GObject_DelRef(prog->res.spr[i]);
        prog->res.spr[i] = NULL;
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
            vcpu_sigsend(self->screenConfigObserver, self->screenConfigObserverSignal);
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
        copper_prog_compile(prog, vc, fb, clut, self->spriteChannel, self->nullSpriteSurface, self->flags.isLightPenEnabled);
    }

    *pOutProg = prog;

    return err;
}

copper_prog_t _Nullable _GraphicsDriver_GetEditableCopperProg(GraphicsDriverRef _Nonnull _Locked self)
{
    copper_prog_t prog = copper_unschedule();

    if (prog == NULL) {
        copper_prog_t run_prog = g_copper_running_prog;
        const errno_t err = _create_copper_prog(self, run_prog->prog_size, &prog);

        if (err != EOK) {
            // should not happen in actual practice because there should always
            // be at least one prog cached.
            return NULL;
        }

        // Accessing the running Copper program without masking irqs is safe here
        // because:
        // * we hold the io_mtx and thus noone else can schedule a new Copper prog
        // * we just took the ready program out and thus the running program
        //   won't get replaced behind our back
        for (size_t i = 0; i < run_prog->prog_size; i++) {
            prog->prog[i] = run_prog->prog[i];
        }

        if (run_prog->even_entry) {
            prog->even_entry = &prog->prog[prog->prog_size / 2];
        }

        prog->loc = run_prog->loc;
        prog->video_conf = run_prog->video_conf;
        prog->res = run_prog->res;

        GObject_AddRef(prog->res.clut);
        if (prog->res.fb) GObject_AddRef(prog->res.fb);

        for (int i = 0; i < SPRITE_COUNT; i++) {
            GObject_AddRef(prog->res.spr[i]);
        }
    }
    return prog;
}
