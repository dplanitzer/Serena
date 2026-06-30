//
//  boot_screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "boot_screen.h"
#include <driver/hw/m68k-amiga/graphics/GraphicsDriver.h>
#include <driver/IORegistry.h>
#include <hal/hw/m68k-amiga/chipset.h>
#include <kpi/fd.h>
#include <kpi/file.h>
#include <string.h>

IOCATS_DEF(g_fb_cats, IOVID_FB);


void bt_open(bt_screen_t* _Nonnull bscr)
{
    decl_try_err();
    GraphicsDriverRef fb = NULL;
    int width, height;
    int srf = -1, clut = -1;

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

    if ((err = IORegistry_OpenBestMatch(gIORegistry, g_fb_cats, O_RDWR, (IODriverRef*)&fb)) == EOK) {
        // Create the surface and screen
        GraphicsDriver_CreateSurface2d(fb, width, height, PIXFMT_RGB_IND_1, &srf);
        GraphicsDriver_CreateCLUT(fb, 32, &clut);


        // Define the screen colors
        static const color_rgb32_t clrs[2] = {
            RGBColor32_Make(0xff, 0xff, 0xff),
            RGBColor32_Make(0x00, 0x00, 0x00)
        };
        GraphicsDriver_SetCLUTEntries(fb, clut, 0, 2, clrs);

        bscr->fb = fb;
        bscr->clut = clut;
        bscr->srf = srf;
        bscr->width = width;
        bscr->height = height;

        GraphicsDriver_ClearPixels(fb, bscr->srf);
        GraphicsDriver_MapSurface(fb, bscr->srf, SURFACE_MAP_RW, &bscr->mp);

        
        // Blit the boot logo
        bt_drawicon(bscr, &g_icon_serena);


        // Show the screen on the monitor
        intptr_t sc[5];
        sc[0] = SCREEN_CONF_FRAMEBUFFER;
        sc[1] = bscr->srf;
        sc[2] = SCREEN_CONF_CLUT;
        sc[3] = bscr->clut;
        sc[4] = SCREEN_CONF_END;
        GraphicsDriver_SetScreenConfig(fb, &sc[0]);
    }
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
        GraphicsDriver_UnmapSurface(bscr->fb, bscr->srf);

        GraphicsDriver_SetScreenConfig(bscr->fb, NULL);
        GraphicsDriver_DestroyCLUT(bscr->fb, bscr->clut);
        GraphicsDriver_DestroySurface(bscr->fb, bscr->srf);

        IODriver_Close(bscr->fb);
        Object_Release(bscr->fb);
    }
}
