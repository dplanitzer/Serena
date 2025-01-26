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
#include "CopperProgram.h"
#include "CopperScheduler.h"
#include "MousePainter.h"
#include "Screen.h"


final_class_ivars(GraphicsDriver, Driver,
    Screen* _Nonnull    screen;
    Sprite* _Nonnull    nullSprite;
    Lock                lock;   // protects the driver and the current screen
    CopperScheduler     copperScheduler;
    InterruptHandlerID  vb_irq_handler;
    Semaphore           vblank_sema;
    bool                isLightPenEnabled;  // Applies to all screens
    MousePainter        mousePainter;
);


extern void _GraphicsDriver_Deinit(GraphicsDriverRef _Nonnull self);

extern void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull self);
extern void GraphicsDriver_StopVideoRefresh_Locked(GraphicsDriverRef _Nonnull self);

extern errno_t GraphicsDriver_SetCurrentScreen_Locked(GraphicsDriverRef _Nonnull self, Screen* _Nonnull pScreen);

#endif /* GraphicsDriverPriv_h */
