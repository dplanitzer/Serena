//
//  gd.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _GD_H
#define _GD_H

#include <ext/try.h>
#include <ext/queue.h>
#include <sched/mtx.h>
#include <sched/sem.h>
#include <sched/vcpu.h>
#include <sched/waitqueue.h>
#include "copper.h"
#include "ColorTable.h"
#include "Surface.h"


extern errno_t gdInit(void);


#define gdLock() \
mtx_lock(&gd_mtx)

#define gdUnlock() \
mtx_unlock(&gd_mtx)


// CLUT
extern errno_t gdGenClut(size_t colorDepth, int* _Nonnull pOutId);
extern errno_t gdDeleteClut(int id);
extern errno_t gdGetClutInfo(int id, clut_info_t* _Nonnull pOutInfo);
extern errno_t gdSetClutEntries(int id, size_t idx, size_t count, const color_rgb32_t* _Nonnull entries);

// Pixel Buffer
extern errno_t gdGenBuffer(int width, int height, pixfmt_t pixelFormat, int* _Nonnull pOutId);
extern errno_t gdDeleteBuffer(int id);
extern errno_t gdGetBufferInfo(int id, surface_info_t* _Nonnull pOutInfo);
extern errno_t gdBindBuffer(int target, int id);
extern errno_t gdMapBuffer(int id, int mode, surface_mapping_t* _Nonnull pOutMapping);
extern errno_t gdUnmapBuffer(int id);
extern errno_t gdWritePixels(int id, const void* _Nonnull planes[], size_t bytesPerRow, pixfmt_t format);
extern errno_t gdClearPixels(int id);

// Sprites
extern errno_t gdSetSpritePos(int spriteId, int x, int y);
extern errno_t gdSetSpriteVis(int spriteId, bool isVisible);
extern void gdGetSpriteCaps(sprite_caps_t* _Nonnull cp);

extern errno_t _gdBindSprite(int unit, Surface* _Nullable srf);

// Mouse Cursor
extern errno_t gdObtainCursor(void);
extern void gdReleaseCursor();
extern errno_t gdBindCursor(int id);
extern void gdSetCursorPos(int x, int y);
extern void gdSetCursorVis(bool isVisible);

// Screen
extern errno_t gdSetScreenConfig(const intptr_t* _Nullable icfg);
extern errno_t gdGetScreenConfig(intptr_t* _Nonnull conf, size_t bufsiz);
extern errno_t gdSetScreenClutEntries(size_t idx, size_t count, const color_rgb32_t* _Nonnull entries);
extern void gdGetScreenSize(int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);
extern void gdSetScreenConfigObserver(vcpu_t _Nullable vp, int signo);
extern void gdSetLightPenEnabled(bool enabled);


//
// Internal
//

#define MAX_CACHED_COPPER_PROGS 4
#define MOUSE_SPRITE_PRI 0


extern mtx_t                    gd_mtx;
extern Surface* _Nonnull        g_null_sprite_surface;
extern sprite_channel_t         g_sprite[SPRITE_COUNT];
extern bool                     g_light_pen_enabled;
extern bool                     g_mouse_cursor_active;
extern vcpu_t _Nullable         g_screen_conf_observer;
extern int                      g_screen_conf_signal;
extern copper_prog_t _Nullable  g_copprg_cache;
extern size_t                   g_copprg_cache_count;
extern vcpu_t _Nonnull          g_copper_vp;
extern struct waitqueue         g_copper_wq;
extern sigset_t                 g_copper_sigs;

extern void gdCopperManager(void*);

// Compiles a Copper program to display the null screen. The null screen shows
// nothing.
extern errno_t create_null_copper_prog(copper_prog_t _Nullable * _Nonnull pOutProg);

// Creates the even and odd field Copper programs for the given screen. There will
// always be at least an odd field program. The even field program will only exist
// for an interlaced screen.
extern errno_t create_screen_copper_prog(const video_conf_t* _Nonnull vc, Surface* _Nonnull srf, ColorTable* _Nonnull clut, copper_prog_t _Nullable * _Nonnull pOutProg);

extern copper_prog_t _Nullable copper_get_editable_prog(void);

#endif /* _GD_H */
