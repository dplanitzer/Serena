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
#include <ext/queue.h>
#include <process/Process.h>
#include <sched/mtx.h>
#include <sched/sem.h>
#include <sched/vcpu.h>
#include <sched/waitqueue.h>
#include "copper.h"
#include "ColorTable.h"
#include "Surface.h"


// Signal sent by the Copper scheduler when a new Copper program has started
// running and the previously running one has been retired
#define SIGCOPRUN  SIGUSR1

#define MOUSE_SPRITE_PRI 0
#define MAX_CACHED_COPPER_PROGS 4


final_class_ivars(GraphicsDriver, DisplayDriver,
    mtx_t                   io_mtx;

    vcpu_t _Nonnull         copvp;
    struct waitqueue        copvpWaitQueue;
    sigset_t                copvpSigs;

    copper_prog_t _Nullable copperProgCache;
    size_t                  copperProgCacheCount;

    vcpu_t _Nullable        screenConfigObserver;
    int                     screenConfigObserverSignal;

    Surface* _Nonnull       nullSpriteSurface;
    sprite_channel_t        spriteChannel[SPRITE_COUNT];

    deque_t/*<GObject>*/    gobjs;
    int                     nextGObjId;

    struct __GDFlags {
        unsigned int    isLightPenEnabled:1;
        unsigned int    isMouseCursorObtained:1;
        unsigned int    reserved:30;
    }                       flags;
);


extern int _GraphicsDriver_GetNewGObjId(GraphicsDriverRef _Nonnull _Locked self);

extern errno_t _GraphicsDriver_CreateSurface2d(GraphicsDriverRef _Nonnull _Locked self, int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSurface);
extern errno_t _GraphicsDriver_CreateCLUT(GraphicsDriverRef _Nonnull _Locked self, size_t colorDepth, RGBColor32 defaultColor, ColorTable* _Nullable * _Nonnull pOutClut);


extern void GraphicsDriver_CopperManager(GraphicsDriverRef _Nonnull self);

// Compiles a Copper program to display the null screen. The null screen shows
// nothing.
extern errno_t GraphicsDriver_CreateNullCopperProg(GraphicsDriverRef _Nonnull _Locked self, copper_prog_t _Nullable * _Nonnull pOutProg);

// Creates the even and odd field Copper programs for the given screen. There will
// always be at least an odd field program. The even field program will only exist
// for an interlaced screen.
extern errno_t GraphicsDriver_CreateScreenCopperProg(GraphicsDriverRef _Nonnull _Locked self, const video_conf_t* _Nonnull vc, Surface* _Nonnull srf, ColorTable* _Nonnull clut, copper_prog_t _Nullable * _Nonnull pOutProg);

extern copper_prog_t _Nullable _GraphicsDriver_GetEditableCopperProg(GraphicsDriverRef _Nonnull _Locked self);


extern void* _Nullable _GraphicsDriver_GetGObjForId(GraphicsDriverRef _Nonnull _Locked self, int id, int type);

#define _GraphicsDriver_GetSurfaceForId(__self, __id) \
(Surface*)_GraphicsDriver_GetGObjForId(__self, __id, kGObject_Surface)

#define _GraphicsDriver_GetClutForId(__self, __id) \
(ColorTable*)_GraphicsDriver_GetGObjForId(__self, __id, kGObject_ColorTable)

extern void _GraphicsDriver_DestroyGObj(GraphicsDriverRef _Nonnull _Locked self, void* gobj);

extern errno_t _GraphicsDriver_BindSprite(GraphicsDriverRef _Nonnull _Locked self, int unit, Surface* _Nullable srf);

// Screens
extern void GraphicsDriver_getScreenSize(GraphicsDriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);
extern void GraphicsDriver_setScreenConfigObserver(GraphicsDriverRef _Nonnull self, vcpu_t _Nullable vp, int signo);


// Light Pen
extern void GraphicsDriver_setLightPenEnabled(GraphicsDriverRef _Nonnull self, bool enabled);


// Mouse Cursor

extern errno_t GraphicsDriver_obtainCursor(GraphicsDriverRef _Nonnull self);
extern void GraphicsDriver_releaseCursor(GraphicsDriverRef _Nonnull self);
extern errno_t GraphicsDriver_bindCursor(GraphicsDriverRef _Nonnull self, int id);
extern void GraphicsDriver_setCursorPosition(GraphicsDriverRef _Nonnull self, int x, int y);
extern void GraphicsDriver_setCursorVisible(GraphicsDriverRef _Nonnull self, bool isVisible);

#endif /* GraphicsDriverPriv_h */
