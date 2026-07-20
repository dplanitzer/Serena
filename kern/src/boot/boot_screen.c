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
    gd_buffer_info_t binf;

    memset(bscr, 0, sizeof(bt_screen_t));
    try(IORegistry_OpenBestMatch(gIORegistry, g_fb_cats, O_RDWR, (IODriverRef*)&drv));


    bscr->drv = drv;
    bscr->buf_id = AGADriver_GetScreenbuffer(drv);
    AGADriver_GetBufferInfo(bscr->drv, bscr->buf_id, &binf);
    bscr->width = binf.width;
    bscr->height = binf.height;


    try(AGADriver_MapBuffer(drv, bscr->buf_id, GD_MAP_RW, &bscr->mp));

        
    // Draw the boot logo
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

    for (int i = 0; i < 3; i++) {
        uint8_t* dp = bscr->mp.plane[i];
        const size_t dbpr = bscr->mp.bytesPerRow;
        const uint8_t* sp = (const uint8_t*)icp->pixels;
        const size_t sbpr = w >> 3;
        const size_t xb = ((bscr->width - w) >> 3) >> 1;
        const size_t yb = (bscr->height - h) >> 1;

        for (size_t y = 0; y < h; y++) {
            memcpy(dp + (y + yb) * dbpr + xb, sp + y * sbpr, sbpr);
        }
    }
}

void bt_close(const bt_screen_t* _Nonnull bscr)
{
    // Remove the screen and turn video off again
    if (bscr->drv) {
        AGADriver_UnmapBuffer(bscr->drv, bscr->buf_id);

        IODriver_Close(bscr->drv);
        Object_Release(bscr->drv);
    }
}
