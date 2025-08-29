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
#include "Screen.h"
#include "Sprite.h"


#define MOUSE_SPRITE_PRI 7

final_class_ivars(GraphicsDriver, Driver,
    mtx_t               io_mtx;
    Screen* _Nonnull    screen;

    Sprite* _Nonnull    spriteChannel[SPRITE_COUNT];
    Sprite              sprite[SPRITE_COUNT];
    Sprite              mouseCursor;
    int16_t             hDiwStart;      // Visible screen space origin and sprite scaling
    int16_t             vDiwStart;
    int16_t             hSprScale;
    int16_t             vSprScale;

    List                surfaces;
    List                screens;
    int                 nextSurfaceId;
    int                 nextScreenId;
    struct __GDFlags {
        unsigned int        isLightPenEnabled;  // Applies to all screens
        unsigned int        mouseCursorEnabled; // Applies to all screens
        unsigned int        isNewCopperProgNeeded;
        unsigned int        reserved:30;
    }                   flags;
);


extern int GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull self);
static Screen* _Nullable _GraphicsDriver_GetScreenForId(GraphicsDriverRef _Nonnull self, int id);

#endif /* GraphicsDriverPriv_h */
