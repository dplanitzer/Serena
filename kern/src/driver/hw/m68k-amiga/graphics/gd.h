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


// Pixel Buffer
extern errno_t gdGenBuffer(int width, int height, vio_pixfmt_t pixelFormat, int* _Nonnull pOutId);
extern errno_t gdDeleteBuffer(int id);
extern errno_t gdGetBufferInfo(int id, vio_buffer_info_t* _Nonnull pOutInfo);
extern errno_t gdBindBuffer(int target, int id);    //XXX
extern errno_t gdMapBuffer(int id, int mode, vio_buffer_data_t* _Nonnull pOutMapping);
extern errno_t gdUnmapBuffer(int id);
extern errno_t gdBufferCommands(int buf_id, int cmds_id, size_t offset);
extern errno_t _gdClearPixels(int id);   // For use by AGADriver (clearing default framebuffer)
extern errno_t _gdDrawPixels(int id, const void* _Nonnull planes[], size_t bytesPerRow, vio_pixfmt_t format);   // For use by HIDDisplay (cursor pixel image update)


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

// Command Buffer
extern errno_t gdGenCmdbuf(size_t reqSize, vio_cmdbuf_desc_t* _Nonnull desc);
extern errno_t gdDeleteCmdbuf(int id);

// Framebuffer
extern errno_t gdGenFramebuffer(size_t colorDepth, int* _Nonnull pOutId);
extern errno_t gdDeleteFramebuffer(int fb_id);
extern errno_t gdAttachBuffer(int fb_id, int buf_id);
extern errno_t gdGetFramebufferInfo(int fb_id, vio_clut_info_t* _Nonnull pOutInfo);
extern errno_t gdSetClutEntries(int fb_id, size_t idx, size_t count, const vio_rgb32_t* _Nonnull entries);
extern errno_t gdSetCurrentFramebuffer(int fb_id);
extern int gdGetCurrentFramebuffer(void);
extern void gdGetScreenSize(int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);
extern void gdSetScreenConfigObserver(vcpu_t _Nullable vp, int signo);
extern void gdSetLightPenEnabled(bool enabled);

// Screen
extern int gdGetScreenbuffer(void);
extern errno_t gdScreenCommands(int id, size_t offset);

#endif /* _GD_H */
