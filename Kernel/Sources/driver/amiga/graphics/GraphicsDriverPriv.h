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
#include <dispatcher/Lock.h>
#include <dispatcher/Semaphore.h>
#include <hal/InterruptController.h>
#include <hal/Platform.h>
#include <klib/List.h>
#include "CopperProgram.h"
#include "CopperScheduler.h"
#include "MousePainter.h"
#include "Screen.h"


final_class_ivars(GraphicsDriver, Driver,
    Screen* _Nonnull    screen;
    Sprite* _Nonnull    nullSprite;
    CopperScheduler     copperScheduler;
    InterruptHandlerID  vb_irq_handler;
    Semaphore           vblank_sema;
    MousePainter        mousePainter;
    List                surfaces;
    List                screens;
    int                 nextSurfaceId;
    int                 nextScreenId;
    bool                isLightPenEnabled;  // Applies to all screens
);


extern void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull self);
static Screen* _Nullable _GraphicsDriver_GetScreenForId(GraphicsDriverRef _Nonnull self, int id);

#endif /* GraphicsDriverPriv_h */
