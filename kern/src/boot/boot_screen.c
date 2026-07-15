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
    AGADriverRef drv = NULL;

    memset(bscr, 0, sizeof(bt_screen_t));
    try(IORegistry_OpenBestMatch(gIORegistry, g_fb_cats, O_RDWR, (IODriverRef*)&drv));


    // Switch to the desired boot video mode
    static const vio_rgb32_t clrs[2] = {
        VIO_RGB32_MAKE(0xff, 0xff, 0xff),
        VIO_RGB32_MAKE(0x00, 0x00, 0x00)
    };
    vio_mode_t mode;

    if (chipset_is_ntsc()) {
        mode.width = 320;
        mode.height = 200;
        
        //mode.width = 320;
        //mode.height = 400;
    } else {
        mode.width = 320;
        mode.height = 256;

        //mode.width = 320;
        //mode.height = 512;
    }
    mode.pixelFormat = VIO_COLOR_INDEX1;
    mode.clear.index = 0;
    mode.paletteSize = 2;
    mode.palette = clrs;


    try(AGADriver_SetVideoMode(drv, &mode, &bscr->buf_id, &bscr->fb_id));


    bscr->drv = drv;
    bscr->width = mode.width;
    bscr->height = mode.height;


    try(AGADriver_MapBuffer(drv, bscr->buf_id, VIO_MAP_RW, &bscr->mp));

        
    // Blit the boot logo
    bt_drawicon(bscr, &g_icon_serena);

catch:
    return err;
}

void bt_drawicon(const bt_screen_t* _Restrict _Nonnull bscr, const bt_icon_t* _Restrict _Nonnull icp)
{
    if (bscr->drv == NULL) {
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
    if (bscr->drv) {
        AGADriver_UnmapBuffer(bscr->drv, bscr->buf_id);

        AGADriver_SetVideoOff(bscr->drv);
        AGADriver_DestroyFramebuffer(bscr->drv, bscr->fb_id);
        AGADriver_DestroyBuffer(bscr->drv, bscr->buf_id);

        IODriver_Close(bscr->drv);
        Object_Release(bscr->drv);
    }
}
