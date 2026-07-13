//
//  gd_copper.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Copper Management
////////////////////////////////////////////////////////////////////////////////


static errno_t _create_copper_prog(size_t instr_count, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    copper_prog_t prog = NULL;
    copper_prog_t cp = g_copprg_cache;
    copper_prog_t pp = NULL;

    // Find a retired program that is big enough to hold 'instr_count' instructions 
    while (cp) {
        if (cp->prog_size >= instr_count) {
            if (pp) {
                pp->next = cp->next;
            }
            else {
                g_copprg_cache = cp->next;
            }

            prog = cp;
            prog->next = NULL;
            g_copprg_cache_count--;
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

static void _cache_copper_prog(copper_prog_t _Nonnull prog)
{
    ColorTable* clut = (ColorTable*)prog->res.clut;
    Surface* fb = (Surface*)prog->res.fb;

    ColorTable_DelRef(clut);
    prog->res.clut = NULL;
    Surface_DelRef(fb);
    prog->res.fb = NULL;

    for (int i = 0; i < SPRITE_COUNT; i++) {
        Surface_DelRef(prog->res.spr[i]);
        prog->res.spr[i] = NULL;
    }


    if (g_copprg_cache_count >= MAX_CACHED_COPPER_PROGS) {
        copper_prog_destroy(prog);
        return;
    }

    if (g_copprg_cache) {
        prog->next = g_copprg_cache;
        g_copprg_cache = prog;
    }
    else {
        prog->next = NULL;
        g_copprg_cache = prog;
    }
    g_copprg_cache_count++;
}

copper_prog_t _Nullable copper_get_editable_prog(void)
{
    copper_prog_t prog = copper_unschedule();

    if (prog == NULL) {
        copper_prog_t run_prog = g_copper_running_prog;
        const errno_t err = _create_copper_prog(run_prog->prog_size, &prog);

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

        if (prog->res.clut) {
            ColorTable_AddRef(prog->res.clut);
        }
        if (prog->res.fb) {
            Surface_AddRef(prog->res.fb);
        }

        for (int i = 0; i < SPRITE_COUNT; i++) {
            Surface_AddRef(prog->res.spr[i]);
        }
    }
    return prog;
}


void gdCopperManager(void* ignore)
{
    gdLock();

    for (;;) {
        copper_prog_t prog;
        int signo;
        bool hasChange = false;

        while ((prog = copper_acquire_retired_prog()) != NULL) {
            _cache_copper_prog(prog);
            hasChange = true;
        }


        if (hasChange && g_screen_conf_observer) {
            vcpu_send_signal(g_screen_conf_observer, g_screen_conf_signal);
        }


        gdUnlock();
        vcpu_sigwait(&g_copper_wq, &g_copper_sigs, 0, TICKS_MAX, &signo);
        gdLock();
    }

    gdUnlock();
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Copper Program Generators
////////////////////////////////////////////////////////////////////////////////

errno_t create_null_copper_prog(copper_prog_t _Nullable * _Nonnull pOutProg)
{
    return create_screen_copper_prog(get_null_video_conf(), NULL, NULL, pOutProg);
}

errno_t create_screen_copper_prog(const video_conf_t* _Nonnull vc, Surface* _Nullable fb, ColorTable* _Nullable clut, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    const size_t instrCount = calc_copper_prog_instruction_count(vc);
    copper_prog_t prog = NULL;

    err = _create_copper_prog(instrCount, &prog);
    if (err == EOK) {
        copper_prog_compile(prog, vc, fb, clut, g_sprite, g_null_sprite_surface, g_light_pen_enabled);
    }

    *pOutProg = prog;

    return err;
}
