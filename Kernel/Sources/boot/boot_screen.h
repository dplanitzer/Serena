//
//  boot_screen.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef boot_screen_h
#define boot_screen_h

#include <kern/types.h>
#include <kpi/fb.h>
#include <kobj/AnyRefs.h>

typedef struct bt_screen {
    IOChannelRef _Nullable  chan;
    int                     clut;
    int                     srf;
    size_t                  width;
    size_t                  height;
    SurfaceMapping          mp;
} bt_screen_t;

typedef struct bt_icon {
    uint16_t * _Nonnull     pixels;
    int16_t                 width;
    int16_t                 height;
} bt_icon_t;


extern void bt_open(bt_screen_t* _Nonnull bscr);
extern void bt_close(const bt_screen_t* _Nonnull bscr);

extern void bt_drawicon(const bt_screen_t* _Nonnull bscr, const bt_icon_t* _Restrict _Nonnull icp);


extern const bt_icon_t g_icon_serena;
extern const bt_icon_t g_icon_floppy;

#endif /* boot_screen_h */
