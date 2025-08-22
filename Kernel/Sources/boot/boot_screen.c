//
//  boot_screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "boot_screen.h"
#include <console/Console.h>
#include <diskcache/DiskCache.h>
#include <driver/DriverChannel.h>
#include <driver/DriverManager.h>
#include <kern/string.h>
#include <kpi/fcntl.h>
#include <machine/amiga/chipset.h>


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

    if ((err = DriverManager_Open(gDriverManager, "/hw/fb", O_RDWR, &chan)) == EOK) {
        // Create the surface and screen
        IOChannel_Ioctl(chan, kFBCommand_CreateSurface, cfg.width, cfg.height, kPixelFormat_RGB_Indexed1, &srf);
        IOChannel_Ioctl(chan, kFBCommand_CreateScreen, &cfg, srf, &scr);


        // Define the screen colors
        static const RGBColor32 clrs[2] = {
            RGBColor32_Make(0xff, 0xff, 0xff),
            RGBColor32_Make(0x00, 0x00, 0x00)
        };
        IOChannel_Ioctl(chan, kFBCommand_SetCLUTEntries, scr, 0, 2, clrs);

        bscr->chan = chan;
        bscr->scr = scr;
        bscr->srf = srf;
        bscr->width = cfg.width;
        bscr->height = cfg.height;

        IOChannel_Ioctl(chan, kFBCommand_MapSurface, bscr->srf, kMapPixels_ReadWrite, &bscr->mp);

        // Blit the boot logo
        clear_boot_screen(bscr);
        blit_boot_logo(bscr, gSerenaImg_Plane0, gSerenaImg_Width, gSerenaImg_Height);


        // Show the screen on the monitor
        IOChannel_Ioctl(chan, kFBCommand_SetCurrentScreen, scr);
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
    if (bscr->chan) {
        IOChannel_Ioctl(bscr->chan, kFBCommand_UnmapSurface, bscr->srf);

        IOChannel_Ioctl(bscr->chan, kFBCommand_SetCurrentScreen, 0);
        IOChannel_Ioctl(bscr->chan, kFBCommand_DestroyScreen, bscr->scr);
        IOChannel_Ioctl(bscr->chan, kFBCommand_DestroySurface, bscr->srf);
        IOChannel_Release(bscr->chan);
    }
}
