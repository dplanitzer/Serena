//
//  boot_screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "boot_screen.h"
#include <driver/hw/m68k-amiga/graphics/AGADriver.h>
#include <driver/IORegistry.h>
#include <hal/hw/m68k-amiga/chipset.h>
#include <kpi/fd.h>
#include <kpi/file.h>
#include <string.h>

IOCATS_DEF(g_fb_cats, IOVID_FB);


errno_t bt_open(bt_screen_t* _Nonnull bscr)
{
    decl_try_err();
    AGADriverRef fb = NULL;
    int width, height;

    if (chipset_is_ntsc()) {
        width = 320;
        height = 200;
        
        //width = 320;
        //height = 400;
    } else {
        width = 320;
        height = 256;

        //width = 320;
        //height = 512;
    }

    memset(bscr, 0, sizeof(bt_screen_t));

    try(IORegistry_OpenBestMatch(gIORegistry, g_fb_cats, O_RDWR, (IODriverRef*)&fb));


    // Allocate all needed resources
    try(AGADriver_CreateCommandBuffer(fb, 128, &bscr->cmdbuf));
    try(AGADriver_CreateBuffer(fb, width, height, VIO_COLOR_INDEX1, &bscr->pbo));
    try(AGADriver_CreateCLUT(fb, 32, &bscr->clut));


    bscr->fb = fb;
    bscr->width = width;
    bscr->height = height;


    // Clear the screen
    static const vio_rgb32_t clrs[2] = {
        VIO_RGB32_MAKE(0xff, 0xff, 0xff),
        VIO_RGB32_MAKE(0x00, 0x00, 0x00)
    };

    void* ip = bscr->cmdbuf.addr;
    ip = vio_set_clut_rgb32(ip, bscr->clut, 0, 2, clrs);
    ip = vio_clear_pixels(ip, bscr->pbo);
    ip = vio_end(ip);

    try(AGADriver_ExecuteCommandBuffer(fb, bscr->cmdbuf.id, 0));
    try(AGADriver_MapBuffer(fb, bscr->pbo, VIO_MAP_RW, &bscr->mp));

        
    // Blit the boot logo
    bt_drawicon(bscr, &g_icon_serena);


    // Show the screen on the monitor
    intptr_t sc[5];
    sc[0] = VIO_SCR_FRAMEBUFFER;
    sc[1] = bscr->pbo;
    sc[2] = VIO_SCR_CLUT;
    sc[3] = bscr->clut;
    sc[4] = VIO_SCR_END;
    err = AGADriver_SetScreenConfig(fb, &sc[0]);

catch:
    return err;
}

void bt_drawicon(const bt_screen_t* _Restrict _Nonnull bscr, const bt_icon_t* _Restrict _Nonnull icp)
{
    if (bscr->fb == NULL) {
        return;
    }

    const size_t w = icp->width;
    const size_t h = icp->height;
    uint8_t* dp = bscr->mp.plane[0];
    const size_t dbpr = bscr->mp.bytesPerRow;
    const uint8_t* sp = (const uint8_t*)icp->pixels;
    const size_t sbpr = w >> 3;
    const size_t xb = ((bscr->width - w) >> 3) >> 1;
    const size_t yb = (bscr->height - h) >> 1;

    for (size_t y = 0; y < h; y++) {
        memcpy(dp + (y + yb) * dbpr + xb, sp + y * sbpr, sbpr);
    }
}

void bt_close(const bt_screen_t* _Nonnull bscr)
{
    // Remove the screen and turn video off again
    if (bscr->fb) {
        AGADriver_UnmapBuffer(bscr->fb, bscr->pbo);

        AGADriver_SetScreenConfig(bscr->fb, NULL);
        AGADriver_DestroyCLUT(bscr->fb, bscr->clut);
        AGADriver_DestroyBuffer(bscr->fb, bscr->pbo);
        AGADriver_DestroyCommandBuffer(bscr->fb, bscr->cmdbuf.id);

        IODriver_Close(bscr->fb);
        Object_Release(bscr->fb);
    }
}
