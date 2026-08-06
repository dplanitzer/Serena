//
//  gd.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _GD_H
#define _GD_H

#include <stdbool.h>
#include <stdint.h>
#include <ext/try.h>
#include <kpi/framebuffer.h>
#include <sched/mtx.h>
#include <sched/vcpu.h>

struct image;

extern errno_t gdInit(void);
extern mtx_t gd_mtx;


#define gdLock() \
mtx_lock(&gd_mtx)

#define gdUnlock() \
mtx_unlock(&gd_mtx)


// Images
extern errno_t gdCreateImage(pid_t pid, int width, int height, gd_pixfmt_t pixelFormat, int* _Nonnull pOutId);
extern errno_t gdDestroyImage(pid_t pid, int id);
extern void gdDestroyImagesOwnedBy(pid_t pid);
extern errno_t gdGetImageInfo(pid_t pid, int id, gd_image_info_t* _Nonnull pOutInfo);
extern errno_t gdBindImage(pid_t pid, int target, int id);    //XXX
extern errno_t gdMapImage(pid_t pid, int id, int mode, gd_image_data_t* _Nonnull pOutMapping);
extern errno_t gdUnmapImage(pid_t pid, int id);
extern errno_t gdClearPixels(pid_t pid, int id);   // For use by AGADriver (clearing default framebuffer)
extern errno_t gdWritePixels(pid_t pid, int id, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format);   // For use by HIDDisplay (cursor pixel image update)

extern errno_t _gdCreateImage(pid_t pid, int width, int height, gd_pixfmt_t pixelFormat, struct image* _Nullable * _Nonnull pOutSelf);
extern void _gdDestroyImage(struct image* _Nonnull self);
extern void _gdClearPixels(struct image* _Nonnull self);
extern errno_t _gdWritePixels(struct image* self, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format);

// Sprites
extern errno_t gdAcquireSprites(pid_t pid, int basePriority, size_t count, int* _Nonnull pOutSpriteIds);
extern errno_t gdReleaseSprites(pid_t pid, const int* _Nullable spriteIds, size_t count);
extern errno_t gdMoveSprite(pid_t pid, int spriteId, int16_t x, int16_t y);
extern errno_t gdShowSprite(pid_t pid, int spriteId, bool isVisible);
extern void gdGetSpriteCaps(gd_sprite_caps_t* _Nonnull cp);

extern errno_t _gdBindSpriteImage(pid_t pid, int spriteId, struct image* _Nullable img);


// Command Buffer
extern errno_t gdCreateCommandBuffer(pid_t pid, size_t reqSize, gd_cmdbuf_desc_t* _Nonnull desc);
extern errno_t gdDestroyCommandBuffer(pid_t pid, int id);
extern void gdDestroyCommandBuffersOwnedBy(pid_t pid);
extern errno_t gdSubmitCommandBuffer(pid_t pid, int img_id, int cmds_id);


// CLUT
extern errno_t gdClut(size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries);
extern errno_t gdGetClut(size_t idx, size_t count, gd_rgb32_t* _Nonnull entries);
extern errno_t gdGetClutInfo(gd_clut_info_t* _Nonnull info);


// Display
extern errno_t gdDisplayMode(const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op);
extern errno_t gdGetDisplayInfo(int flavor, gd_display_info_ref_t _Nonnull info);
extern errno_t gdEnumDisplayModes(int index, gd_display_mode_t* _Nonnull pOutMode);
extern void gdSetScreenConfigObserver(vcpu_t _Nullable vp, int signo);
extern void gdSetLightPenEnabled(bool enabled);

#endif /* _GD_H */
