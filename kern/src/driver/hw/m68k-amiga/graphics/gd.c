//
//  gd.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <kern/kalloc.h>

mtx_t   gd_mtx;
bool    g_light_pen_enabled;


errno_t gdInit(void)
{
    decl_try_err();

    mtx_init(&gd_mtx);


    // Create a null sprite
    try(kalloc_options(sizeof(uint16_t) * 6, KALLOC_OPTION_UNIFIED, (void**)&g_null_sprite_data));
    g_null_sprite_data[0] = 0x1905;
    g_null_sprite_data[1] = 0x1a00;
    g_null_sprite_data[2] = 0;
    g_null_sprite_data[3] = 0;
    g_null_sprite_data[4] = 0;
    g_null_sprite_data[5] = 0;

    for (int i = 0; i < SPRITE_COUNT; i++) {
        g_sprite[i].isVisible = true;
        g_sprite[i].id = i;
    }


    // Allocate the Copper manager
    try(_gdInitCopper());
    
catch:
    return err;
}
