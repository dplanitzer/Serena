//
//  gd.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd.h"

mtx_t                       gd_mtx;
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
    for (int i = 0; i < SPRITE_COUNT; i++) {
        g_sprite[i].isVisible = true;
    }


    // Allocate the Copper manager
    try(_gdInitCopper());
    
catch:
    return err;
}
