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
static const vio_rgb32_t ansi_clrs[ANSI_COLOR_COUNT] = {
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
    

    // Initialize the boot screen buffer
    int width, height;

    if (chipset_is_ntsc()) {
        width = 640;
        height = 200;
        
        //width = 640;
        //height = 400;
    } else {
        width = 640;
        height = 256;

        //width = 640;
        //height = 512;
    }

    int buf_id, fb_id;

    try(gdGenBuffer(width, height, VIO_COLOR_INDEX3, &buf_id));
    _gdClearPixels(buf_id);


    try(gdGenFramebuffer(32, &fb_id));
    try(gdAttachBuffer(fb_id, buf_id));
    gdSetClutEntries(fb_id, 0, ANSI_COLOR_COUNT, ansi_clrs);


    err = gdSetCurrentFramebuffer(fb_id);

catch:
    return err;
}
