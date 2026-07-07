//
//  AGADriverPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef AGADriverPriv_h
#define AGADriverPriv_h

#include "AGADriver.h"
#include <ext/queue.h>
#include <sched/mtx.h>
#include <sched/sem.h>
#include <sched/vcpu.h>
#include <sched/waitqueue.h>
#include "copper.h"
#include "ColorTable.h"
#include "Surface.h"


// Signal sent by the Copper scheduler when a new Copper program has started
// running and the previously running one has been retired
#define SIGCOPRUN  SIG_USER_1

#define MOUSE_SPRITE_PRI 0
#define MAX_CACHED_COPPER_PROGS 4


final_class_ivars(AGADriver, DisplayDriver,
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

    struct __GDFlags {
        unsigned int    isLightPenEnabled:1;
        unsigned int    isMouseCursorObtained:1;
        unsigned int    reserved:30;
    }                       flags;
);


extern errno_t _AGADriver_CreateSurface2d(AGADriverRef _Nonnull _Locked self, int width, int height, pixfmt_t pixelFormat, Surface* _Nullable * _Nonnull pOutSurface);
extern errno_t _AGADriver_CreateCLUT(AGADriverRef _Nonnull _Locked self, size_t colorDepth, color_rgb32_t defaultColor, ColorTable* _Nullable * _Nonnull pOutClut);


extern void AGADriver_CopperManager(AGADriverRef _Nonnull self);

// Compiles a Copper program to display the null screen. The null screen shows
// nothing.
extern errno_t AGADriver_CreateNullCopperProg(AGADriverRef _Nonnull _Locked self, copper_prog_t _Nullable * _Nonnull pOutProg);

// Creates the even and odd field Copper programs for the given screen. There will
// always be at least an odd field program. The even field program will only exist
// for an interlaced screen.
extern errno_t AGADriver_CreateScreenCopperProg(AGADriverRef _Nonnull _Locked self, const video_conf_t* _Nonnull vc, Surface* _Nonnull srf, ColorTable* _Nonnull clut, copper_prog_t _Nullable * _Nonnull pOutProg);

extern copper_prog_t _Nullable _AGADriver_GetEditableCopperProg(AGADriverRef _Nonnull _Locked self);


extern errno_t _AGADriver_BindSprite(AGADriverRef _Nonnull _Locked self, int unit, Surface* _Nullable srf);

// Screens
extern void AGADriver_getScreenSize(AGADriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);
extern void AGADriver_setScreenConfigObserver(AGADriverRef _Nonnull self, vcpu_t _Nullable vp, int signo);


// Light Pen
extern void AGADriver_setLightPenEnabled(AGADriverRef _Nonnull self, bool enabled);


// Mouse Cursor

extern errno_t AGADriver_obtainCursor(AGADriverRef _Nonnull self);
extern void AGADriver_releaseCursor(AGADriverRef _Nonnull self);
extern errno_t AGADriver_bindCursor(AGADriverRef _Nonnull self, int id);
extern void AGADriver_setCursorPosition(AGADriverRef _Nonnull self, int x, int y);
extern void AGADriver_setCursorVisible(AGADriverRef _Nonnull self, bool isVisible);

#endif /* AGADriverPriv_h */
