//
//  gd.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd.h"
#include "copper.h"
#include <driver/IOLib.h>

// Signal sent by the Copper scheduler when a new Copper program has started
// running and the previously running one has been retired
#define SIG_COPRUN  SIG_USER_1


mtx_t                       gd_mtx;
copper_prog_t _Nullable     g_copprg_cache;
size_t                      g_copprg_cache_count;
vcpu_t _Nonnull             g_copper_vp;
struct waitqueue            g_copper_wq;
sigset_t                    g_copper_sigs;
vcpu_t _Nullable            g_screen_conf_observer;
int                         g_screen_conf_signal;
Surface* _Nonnull           g_null_sprite_surface;
sprite_channel_t            g_sprite[SPRITE_COUNT];
bool                        g_light_pen_enabled;
bool                        g_mouse_cursor_active;


errno_t gdInit(void)
{
    decl_try_err();

    mtx_init(&gd_mtx);


    // Create a null Copper program and null sprite
    copper_prog_t nullCopperProg;
    try(Surface_CreateNullSprite(&g_null_sprite_surface));
    try(create_null_copper_prog(&nullCopperProg));
    for (int i = 0; i < SPRITE_COUNT; i++) {
        g_sprite[i].isVisible = true;
    }


    // Allocate the Copper management VCPU
    wq_init(&g_copper_wq);
    g_copper_sigs = sig_bit(SIG_COPRUN);

    try(IOAcquireVirtualProcessor((vcpu_func_t)gdCopperManager, NULL, VCPU_QOS_URGENT, VCPU_PRI_NORMAL, &g_copper_vp));

    
    // Initialize the Copper scheduler
    copper_init(nullCopperProg, SIG_COPRUN, g_copper_vp);
    copper_start();

    IOResumeVirtualProcessor(g_copper_vp);
    
catch:
    return err;
}
