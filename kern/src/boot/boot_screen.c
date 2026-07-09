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


void bt_open(bt_screen_t* _Nonnull bscr)
{
    decl_try_err();
    AGADriverRef fb = NULL;
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
        AGADriver_CreateBuffer(fb, width, height, PIXFMT_RGB_IND_1, &srf);
        AGADriver_CreateCLUT(fb, 32, &clut);


        // Define the screen colors
        static const color_rgb32_t clrs[2] = {
            RGBColor32_Make(0xff, 0xff, 0xff),
            RGBColor32_Make(0x00, 0x00, 0x00)
        };
        AGADriver_SetCLUTEntries(fb, clut, 0, 2, clrs);

        bscr->fb = fb;
        bscr->clut = clut;
        bscr->srf = srf;
        bscr->width = width;
        bscr->height = height;

        AGADriver_ClearPixels(fb, bscr->srf);
        AGADriver_MapBuffer(fb, bscr->srf, BUFFER_MAP_RW, &bscr->mp);

        
        // Blit the boot logo
        bt_drawicon(bscr, &g_icon_serena);


        // Show the screen on the monitor
        intptr_t sc[5];
        sc[0] = SCREEN_CONF_FRAMEBUFFER;
        sc[1] = bscr->srf;
        sc[2] = SCREEN_CONF_CLUT;
        sc[3] = bscr->clut;
        sc[4] = SCREEN_CONF_END;
        AGADriver_SetScreenConfig(fb, &sc[0]);
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
        AGADriver_UnmapBuffer(bscr->fb, bscr->srf);

        AGADriver_SetScreenConfig(bscr->fb, NULL);
        AGADriver_DestroyCLUT(bscr->fb, bscr->clut);
        AGADriver_DestroyBuffer(bscr->fb, bscr->srf);

        IODriver_Close(bscr->fb);
        Object_Release(bscr->fb);
    }
}
