//
//  GraphicsDriverPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef GraphicsDriverPriv_h
#define GraphicsDriverPriv_h

#include "GraphicsDriver.h"
#include <machine/amiga/chipset.h>
#include <machine/irq.h>
#include <klib/List.h>
#include <sched/mtx.h>
#include <sched/sem.h>
#include "copper.h"
#include "ColorTable.h"
#include "Sprite.h"
#include "Surface.h"


typedef struct copper_params {
    Surface* _Nonnull       fb;
    ColorTable* _Nonnull    clut;
    uint16_t** _Nonnull     sprdma;
    bool                    isHires;
    bool                    isLace;
    bool                    isPal;
    bool                    isLightPenEnabled;
} copper_params_t;


typedef struct screen_conf {
    Surface* _Nullable      fb;
    ColorTable* _Nullable   clut;
    int                     fps;
} screen_conf_t;


#define MAX_PIXEL_FORMATS   5
typedef struct hw_conf {
    int16_t     width;
    int16_t     height;
    int8_t      fps;
    int8_t      pixelFormatCount;   // Number of supported pixel formats
    PixelFormat pixelFormat[MAX_PIXEL_FORMATS];
} hw_conf_t;


#define MOUSE_SPRITE_PRI 7

final_class_ivars(GraphicsDriver, Driver,
    mtx_t               io_mtx;

    Surface* _Nullable      fb;
    ColorTable* _Nullable   clut;
    void* _Nullable         hwc;

    uint16_t* _Nonnull  nullSpriteData;
    uint16_t* _Nonnull  spriteDmaPtr[SPRITE_COUNT];
    Sprite              sprite[SPRITE_COUNT];
    Sprite              mouseCursor;
    int16_t             hDiwStart;      // Visible screen space origin and sprite scaling
    int16_t             vDiwStart;
    int16_t             hSprScale;
    int16_t             vSprScale;

    List                surfaces;
    List                colorTables;
    List                screens;
    int                 nextSurfaceId;
    int                 nextScreenId;
    int                 nextClutId;
    struct __GDFlags {
        unsigned int        isLightPenEnabled;  // Applies to all screens
        unsigned int        mouseCursorEnabled; // Applies to all screens
        unsigned int        isNewCopperProgNeeded;
        unsigned int        reserved:30;
    }                   flags;
);


// Compiles a Copper program to display the null screen. The null screen shows
// nothing.
extern errno_t GraphicsDriver_CreateNullCopperProg(GraphicsDriverRef _Nonnull _Locked self, copper_prog_t _Nullable * _Nonnull pOutProg);

// Creates the even and odd field Copper programs for the given screen. There will
// always be at least an odd field program. The even field program will only exist
// for an interlaced screen.
extern errno_t GraphicsDriver_CreateCopperScreenProg(GraphicsDriverRef _Nonnull self, const hw_conf_t* _Nonnull hwc, Surface* _Nonnull srf, ColorTable* _Nonnull clut, copper_prog_t _Nullable * _Nonnull pOutProg);


extern Surface* _Nullable _GraphicsDriver_GetSurfaceForId(GraphicsDriverRef _Nonnull self, int id);
extern ColorTable* _Nullable _GraphicsDriver_GetClutForId(GraphicsDriverRef _Nonnull self, int id);

#endif /* GraphicsDriverPriv_h */
