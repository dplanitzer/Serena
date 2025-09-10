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
#include <machine/amiga/graphics/GraphicsDriver.h>

typedef struct boot_screen {
    IOChannelRef _Nullable  chan;
    int                     clut;
    int                     srf;
    size_t                  width;
    size_t                  height;
    SurfaceMapping          mp;
} boot_screen_t;


extern void open_boot_screen(boot_screen_t* _Nonnull bscr);
extern void close_boot_screen(const boot_screen_t* _Nonnull bscr);

extern void blit_boot_logo(const boot_screen_t* _Nonnull bscr, const uint16_t* _Nonnull bitmap, size_t w, size_t h);


extern const uint16_t gSerenaImg_Plane0[];
extern const int gSerenaImg_Width;
extern const int gSerenaImg_Height;

extern const uint16_t gFloppyImg_Plane0[];
extern const int gFloppyImg_Width;
extern const int gFloppyImg_Height;

#endif /* boot_screen_h */
