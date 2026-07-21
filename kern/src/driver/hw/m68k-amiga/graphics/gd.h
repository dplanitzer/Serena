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
extern errno_t gdGenBuffer(int width, int height, gd_pixfmt_t pixelFormat, int* _Nonnull pOutId);
extern errno_t gdDeleteBuffer(int id);
extern errno_t gdGetBufferInfo(int id, gd_buffer_info_t* _Nonnull pOutInfo);
extern errno_t gdBindBuffer(int target, int id);    //XXX
extern errno_t gdMapBuffer(int id, int mode, gd_buffer_data_t* _Nonnull pOutMapping);
extern errno_t gdUnmapBuffer(int id);
extern errno_t gdBufferCommands(int buf_id, int cmds_id, size_t offset);
extern errno_t _gdClearPixels(int id);   // For use by AGADriver (clearing default framebuffer)
extern errno_t _gdDrawPixels(int id, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format);   // For use by HIDDisplay (cursor pixel image update)


// Sprites
extern errno_t gdSetSpritePos(int spriteId, int x, int y);
extern errno_t gdSetSpriteVis(int spriteId, bool isVisible);
extern void gdGetSpriteCaps(gd_sprite_caps_t* _Nonnull cp);

extern errno_t _gdBindSpriteBuffer(int unit, Surface* _Nullable srf);


// Command Buffer
extern errno_t gdGenCmdbuf(size_t reqSize, gd_cmdbuf_desc_t* _Nonnull desc);
extern errno_t gdDeleteCmdbuf(int id);


// CLUT
extern errno_t gdClut(size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries);
extern errno_t gdGetClut(size_t idx, size_t count, gd_rgb32_t* _Nonnull entries);
extern errno_t gdGetClutInfo(gd_clut_info_t* _Nonnull info);


// Display
extern errno_t gdDisplayMode(const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op);
extern errno_t gdGetDisplayInfo(int flavor, gd_display_info_ref_t _Nonnull info);
extern errno_t gdDisplayCommands(int id, size_t offset);
extern void gdSetScreenConfigObserver(vcpu_t _Nullable vp, int signo);
extern void gdSetLightPenEnabled(bool enabled);


// Mouse Cursor
extern errno_t gdObtainCursor(void);
extern void gdReleaseCursor();
extern errno_t gdBindCursor(int id);
extern void gdSetCursorPos(int x, int y);
extern void gdSetCursorVis(bool isVisible);

#endif /* _GD_H */
