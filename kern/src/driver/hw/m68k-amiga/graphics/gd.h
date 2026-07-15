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
#include <sched/mtx.h>
#include <sched/vcpu.h>
#include "Surface.h"


extern errno_t gdInit(void);
extern mtx_t gd_mtx;


#define gdLock() \
mtx_lock(&gd_mtx)

#define gdUnlock() \
mtx_unlock(&gd_mtx)


// CLUT
extern errno_t gdGenFramebuffer(size_t colorDepth, int* _Nonnull pOutId);
extern errno_t gdDeleteFramebuffer(int id);
extern errno_t gdGetFramebufferInfo(int id, vio_clut_info_t* _Nonnull pOutInfo);
extern errno_t gdSetClutEntries(int id, size_t idx, size_t count, const vio_rgb32_t* _Nonnull entries);

// Pixel Buffer
extern errno_t gdGenBuffer(int width, int height, vio_pixfmt_t pixelFormat, int* _Nonnull pOutId);
extern errno_t gdDeleteBuffer(int id);
extern errno_t gdGetBufferInfo(int id, vio_buffer_info_t* _Nonnull pOutInfo);
extern errno_t gdBindBuffer(int target, int id);
extern errno_t gdMapBuffer(int id, int mode, vio_buffer_data_t* _Nonnull pOutMapping);
extern errno_t gdUnmapBuffer(int id);
extern errno_t gdWritePixels(int id, const void* _Nonnull planes[], size_t bytesPerRow, vio_pixfmt_t format);
extern errno_t gdClearPixels(int id);

// Sprites
extern errno_t gdSetSpritePos(int spriteId, int x, int y);
extern errno_t gdSetSpriteVis(int spriteId, bool isVisible);
extern void gdGetSpriteCaps(vio_sprite_caps_t* _Nonnull cp);

extern errno_t _gdBindSpriteBuffer(int unit, Surface* _Nullable srf);

// Mouse Cursor
extern errno_t gdObtainCursor(void);
extern void gdReleaseCursor();
extern errno_t gdBindCursor(int id);
extern void gdSetCursorPos(int x, int y);
extern void gdSetCursorVis(bool isVisible);

// Screen
extern errno_t gdSetScreenConfig(const intptr_t* _Nullable icfg);
extern errno_t gdGetScreenConfig(intptr_t* _Nonnull conf, size_t bufsiz);
extern void gdGetScreenSize(int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);
extern void gdSetScreenConfigObserver(vcpu_t _Nullable vp, int signo);
extern void gdSetLightPenEnabled(bool enabled);

// Command Buffer
extern errno_t gdGenCmdbuf(size_t reqSize, vio_cmdbuf_desc_t* _Nonnull desc);
extern errno_t gdDeleteCmdbuf(int id);
extern errno_t gdExecCmdbuf(int id, size_t offset);

#endif /* _GD_H */
