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

typedef struct boot_screen {
    IOChannelRef _Nullable  chan;
    int                     clut;
    int                     srf;
    size_t                  width;
    size_t                  height;
    SurfaceMapping          mp;
} boot_screen_t;


extern void bs_open(boot_screen_t* _Nonnull bscr);
extern void bs_close(const boot_screen_t* _Nonnull bscr);

extern void bs_copypixels(const boot_screen_t* _Nonnull bscr, const uint16_t* _Nonnull bitmap, size_t w, size_t h);


extern const uint16_t g_icon_serena_planes[];
extern const int g_icon_serena_width;
extern const int g_icon_serena_height;

extern const uint16_t g_icon_floppy_planes[];
extern const int g_icon_floppy_width;
extern const int g_icon_floppy_height;

#endif /* boot_screen_h */
