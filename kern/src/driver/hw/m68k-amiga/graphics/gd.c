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

#define ANSI_COLOR_COUNT    8
static const gd_rgb32_t ansi_clrs[ANSI_COLOR_COUNT] = {
    0xff000000,     // Black
    0xffff0000,     // Red
    0xff00ff00,     // Green
    0xffffff00,     // Yellow
    0xff0000ff,     // Blue
    0xffff00ff,     // Magenta
    0xff00ffff,     // Cyan
    0xffffffff,     // White
};


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
    

    // Initialize the boot display
    gd_display_mode_t mode;

    if (chipset_is_ntsc()) {
        mode.width = 640;
        mode.height = 200;
        
        //mode.width = 640;
        //mode.height = 400;
    } else {
        mode.width = 640;
        mode.height = 256;

        //mode.width = 640;
        //mode.height = 512;
    }
    mode.pixelFormat = GD_COLOR_INDEX3;
    mode.refreshRate = 60;      //XXX revisit

    err = gdDisplayMode(&mode, NULL, 2);    //XXX 2 (to clear the fb) -> revisit
    gdClut(0, ANSI_COLOR_COUNT, ansi_clrs);

catch:
    return err;
}
