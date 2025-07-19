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
#include <dispatcher/Semaphore.h>
#include <machine/InterruptController.h>
#include <machine/Platform.h>
#include <klib/List.h>
#include <sched/mtx.h>
#include "CopperProgram.h"
#include "CopperScheduler.h"
#include "Screen.h"


final_class_ivars(GraphicsDriver, Driver,
    Screen* _Nonnull    screen;
    Sprite* _Nonnull    nullSprite;
    Sprite* _Nonnull    mouseCursor;
    CopperScheduler     copperScheduler;
    InterruptHandlerID  vb_irq_handler;
    Semaphore           vblank_sema;
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


extern void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull self);
static Screen* _Nullable _GraphicsDriver_GetScreenForId(GraphicsDriverRef _Nonnull self, int id);

#endif /* GraphicsDriverPriv_h */
