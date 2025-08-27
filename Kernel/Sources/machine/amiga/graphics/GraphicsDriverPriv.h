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


final_class_ivars(GraphicsDriver, Driver,
    mtx_t               io_mtx;
    Screen* _Nonnull    screen;
    Sprite* _Nonnull    nullSprite;
    Sprite* _Nonnull    mouseCursor;
    List                surfaces;
    List                screens;
    int                 nextSurfaceId;
    int                 nextScreenId;
    int16_t             mouseCursorRectX;   // Visible screen space origin and mouse cursor scaling
    int16_t             mouseCursorRectY;
    int16_t             mouseCursorScaleX;
    int16_t             mouseCursorScaleY;
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
