//
//  boot_screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "boot_screen.h"
#include <Catalog.h>
#include <console/Console.h>
#include <diskcache/DiskCache.h>
#include <driver/DriverChannel.h>
#include <machine/Platform.h>
#include <kern/string.h>
#include <kpi/fcntl.h>


void open_boot_screen(boot_screen_t* _Nonnull bscr)
{
    decl_try_err();
    GraphicsDriverRef gd = NULL;
    IOChannelRef chan = NULL;
    VideoConfiguration cfg;
    int srf = -1, scr = -1;

    if (chipset_is_ntsc()) {
        cfg.width = 320;
        cfg.height = 200;
        cfg.fps = 60;
        
        //cfg.width = 320;
        //cfg.height = 400;
        //cfg.fps = 30;
    } else {
        cfg.width = 320;
        cfg.height = 256;
        cfg.fps = 50;

        //cfg.width = 320;
        //cfg.height = 512;
        //cfg.fps = 25;
    }

    memset(bscr, 0, sizeof(boot_screen_t));

    if ((err = Catalog_Open(gDriverCatalog, "/hw/fb", O_RDWR, &chan)) == EOK) {
        gd = DriverChannel_GetDriverAs(chan, GraphicsDriver);

        // Create the surface and screen
        GraphicsDriver_CreateSurface(gd, cfg.width, cfg.height, kPixelFormat_RGB_Indexed1, &srf);
        GraphicsDriver_CreateScreen(gd, &cfg, srf, &scr);


        // Define the screen colors
        GraphicsDriver_SetCLUTEntry(gd, scr, 0, RGBColor32_Make(0xff, 0xff, 0xff));
        GraphicsDriver_SetCLUTEntry(gd, scr, 1, RGBColor32_Make(0x00, 0x00, 0x00));

        bscr->gd = gd;
        bscr->chan = chan;
        bscr->scr = scr;
        bscr->srf = srf;
        bscr->width = cfg.width;
        bscr->height = cfg.height;

        GraphicsDriver_MapSurface(bscr->gd, bscr->srf, kMapPixels_ReadWrite, &bscr->mp);

        // Blit the boot logo
        clear_boot_screen(bscr);
        blit_boot_logo(bscr, gSerenaImg_Plane0, gSerenaImg_Width, gSerenaImg_Height);


        // Show the screen on the monitor
        GraphicsDriver_SetCurrentScreen(gd, scr);
    }
}

void clear_boot_screen(const boot_screen_t* _Nonnull bscr)
{
    if (bscr->chan == NULL) {
        return;
    }

    memset(bscr->mp.plane[0], 0, bscr->mp.bytesPerRow[0] * bscr->height);
}

void blit_boot_logo(const boot_screen_t* _Nonnull bscr, const uint16_t* _Nonnull bitmap, size_t w, size_t h)
{
    if (bscr->chan == NULL) {
        return;
    }

    uint8_t* dp = bscr->mp.plane[0];
    const size_t dbpr = bscr->mp.bytesPerRow[0];
    const uint8_t* sp = (const uint8_t*)bitmap;
    const size_t sbpr = w >> 3;
    const size_t xb = ((bscr->width - w) >> 3) >> 1;
    const size_t yb = (bscr->height - h) >> 1;

    for (size_t y = 0; y < h; y++) {
        memcpy(dp + (y + yb) * dbpr + xb, sp + y * sbpr, sbpr);
    }
}

void close_boot_screen(const boot_screen_t* _Nonnull bscr)
{
    // Remove the screen and turn video off again
    if (bscr->gd) {
        GraphicsDriver_UnmapSurface(bscr->gd, bscr->srf);

        GraphicsDriver_SetCurrentScreen(bscr->gd, 0);
        GraphicsDriver_DestroyScreen(bscr->gd, bscr->scr);
        GraphicsDriver_DestroySurface(bscr->gd, bscr->srf);
        IOChannel_Release(bscr->chan);
    }
}
