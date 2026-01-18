//
//  boot_screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "boot_screen.h"
#include <string.h>
#include <driver/DriverChannel.h>
#include <driver/DriverManager.h>
#include <kpi/fcntl.h>
#include <hal/hw/m68k-amiga/chipset.h>


void bt_open(bt_screen_t* _Nonnull bscr)
{
    decl_try_err();
    GraphicsDriverRef gd = NULL;
    IOChannelRef chan = NULL;
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

    if ((err = DriverManager_Open(gDriverManager, "/hw/fb", O_RDWR, &chan)) == EOK) {
        // Create the surface and screen
        IOChannel_Ioctl(chan, kFBCommand_CreateSurface2d, width, height, kPixelFormat_RGB_Indexed1, &srf);
        IOChannel_Ioctl(chan, kFBCommand_CreateCLUT, 32, &clut);


        // Define the screen colors
        static const RGBColor32 clrs[2] = {
            RGBColor32_Make(0xff, 0xff, 0xff),
            RGBColor32_Make(0x00, 0x00, 0x00)
        };
        IOChannel_Ioctl(chan, kFBCommand_SetCLUTEntries, clut, 0, 2, clrs);

        bscr->chan = chan;
        bscr->clut = clut;
        bscr->srf = srf;
        bscr->width = width;
        bscr->height = height;

        IOChannel_Ioctl(chan, kFBCommand_ClearPixels, bscr->srf);
        IOChannel_Ioctl(chan, kFBCommand_MapSurface, bscr->srf, kMapPixels_ReadWrite, &bscr->mp);

        
        // Blit the boot logo
        bt_drawicon(bscr, &g_icon_serena);


        // Show the screen on the monitor
        intptr_t sc[5];
        sc[0] = SCREEN_CONF_FRAMEBUFFER;
        sc[1] = bscr->srf;
        sc[2] = SCREEN_CONF_CLUT;
        sc[3] = bscr->clut;
        sc[4] = SCREEN_CONF_END;
        IOChannel_Ioctl(chan, kFBCommand_SetScreenConfig, &sc[0]);
    }
}

void bt_drawicon(const bt_screen_t* _Restrict _Nonnull bscr, const bt_icon_t* _Restrict _Nonnull icp)
{
    if (bscr->chan == NULL) {
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
    if (bscr->chan) {
        IOChannel_Ioctl(bscr->chan, kFBCommand_UnmapSurface, bscr->srf);

        IOChannel_Ioctl(bscr->chan, kFBCommand_SetScreenConfig, NULL);
        IOChannel_Ioctl(bscr->chan, kFBCommand_DestroyCLUT, bscr->clut);
        IOChannel_Ioctl(bscr->chan, kFBCommand_DestroySurface, bscr->srf);
        IOChannel_Release(bscr->chan);
    }
}
