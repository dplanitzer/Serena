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
#include <klib/List.h>
#include <process/Process.h>
#include <sched/mtx.h>
#include <sched/sem.h>
#include <sched/vcpu.h>
#include <sched/waitqueue.h>
#include "copper.h"
#include "ColorTable.h"
#include "Sprite.h"
#include "Surface.h"


// Signal sent by the Copper scheduler when a new Copper program has started
// running and the previously running one has been retired
#define SIGCOPRUN  2


typedef struct screen_conf {
    Surface* _Nullable      fb;
    ColorTable* _Nullable   clut;
} screen_conf_t;


#define MOUSE_SPRITE_PRI 7
#define MAX_CACHED_COPPER_PROGS 4

final_class_ivars(GraphicsDriver, Driver,
    mtx_t                   io_mtx;

    vcpu_t _Nonnull         copvp;
    struct waitqueue        copvpWaitQueue;
    sigset_t                copvpSigs;

    copper_prog_t _Nullable copperProgCache;
    size_t                  copperProgCacheCount;

    uint16_t* _Nonnull      nullSpriteData;
    uint16_t* _Nonnull      spriteDmaPtr[SPRITE_COUNT];
    Sprite                  sprite[SPRITE_COUNT];
    Sprite                  mouseCursor;

    List/*<GObject>*/       gobjs;
    int                     nextGObjId;
    struct __GDFlags {
        unsigned int    isLightPenEnabled;  // Applies to all screens
        unsigned int    mouseCursorEnabled; // Applies to all screens
        unsigned int    isNewCopperProgNeeded;
        unsigned int    reserved:30;
    }                       flags;
);


extern void GraphicsDriver_CopperManager(GraphicsDriverRef _Nonnull self);

// Compiles a Copper program to display the null screen. The null screen shows
// nothing.
extern errno_t GraphicsDriver_CreateNullCopperProg(GraphicsDriverRef _Nonnull _Locked self, copper_prog_t _Nullable * _Nonnull pOutProg);

// Creates the even and odd field Copper programs for the given screen. There will
// always be at least an odd field program. The even field program will only exist
// for an interlaced screen.
extern errno_t GraphicsDriver_CreateScreenCopperProg(GraphicsDriverRef _Nonnull _Locked self, const video_conf_t* _Nonnull vc, Surface* _Nonnull srf, ColorTable* _Nonnull clut, copper_prog_t _Nullable * _Nonnull pOutProg);


extern void* _Nullable _GraphicsDriver_GetGObjForId(GraphicsDriverRef _Nonnull _Locked self, int id, int type);

#define _GraphicsDriver_GetSurfaceForId(__self, __id) \
(Surface*)_GraphicsDriver_GetGObjForId(__self, __id, kGObject_Surface)

#define _GraphicsDriver_GetClutForId(__self, __id) \
(ColorTable*)_GraphicsDriver_GetGObjForId(__self, __id, kGObject_ColorTable)

extern void _GraphicsDriver_DestroyGObj(GraphicsDriverRef _Nonnull _Locked self, void* gobj);

#endif /* GraphicsDriverPriv_h */
