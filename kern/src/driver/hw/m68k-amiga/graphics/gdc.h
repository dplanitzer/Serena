//
//  gdc.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _GDC_H
#define _GDC_H

#include <stdbool.h>
#include <stdint.h>
#include <driver/hid/IOHIDDisplay.h>
#include <ext/try.h>
#include <kpi/framebuffer.h>
#include <sched/mtx.h>
#include <sched/vcpu.h>

struct image;

extern errno_t gdcInit(void);
extern mtx_t gdc_mtx;


#define gdcLock() \
mtx_lock(&gdc_mtx)

#define gdcUnlock() \
mtx_unlock(&gdc_mtx)


// Images
extern errno_t gdcCreateImage(pid_t pid, int width, int height, gd_pixfmt_t pixelFormat, int* _Nonnull pOutId);
extern errno_t gdcDestroyImage(pid_t pid, int id);
extern void gdcDestroyImagesOwnedBy(pid_t pid);
extern errno_t gdcGetImageInfo(pid_t pid, int id, gd_image_info_t* _Nonnull pOutInfo);
extern errno_t gdcMapImage(pid_t pid, int id, int mode, gd_image_data_t* _Nonnull pOutMapping);
extern errno_t gdcUnmapImage(pid_t pid, int id);
extern errno_t gdcClearPixels(pid_t pid, int id);   // For use by AGADriver (clearing default framebuffer)
extern errno_t gdcWritePixels(pid_t pid, int id, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format);   // For use by HIDDisplay (cursor pixel image update)

extern errno_t _gdcCreateImage(pid_t pid, int width, int height, gd_pixfmt_t pixelFormat, struct image* _Nullable * _Nonnull pOutSelf);
extern void _gdcDestroyImage(struct image* _Nonnull self);
extern void _gdcClearPixels(struct image* _Nonnull self);


// Sprites
extern errno_t gdcAcquireSprites(pid_t pid, int basePriority, size_t count, int* _Nonnull pOutSpriteIds);
extern errno_t gdcReleaseSprites(pid_t pid, const int* _Nullable spriteIds, size_t count);
extern errno_t gdcBindSpriteImage(pid_t pid, int target, int id);    //XXX
extern errno_t gdcMoveSprite(pid_t pid, int spriteId, int16_t x, int16_t y);
extern errno_t gdcShowSprite(pid_t pid, int spriteId, bool isVisible);
extern void gdcGetSpriteConstraints(gd_sprite_constraints_t* _Nonnull cp);

extern errno_t _gdcBindSpriteImage(pid_t pid, int spriteId, struct image* _Nullable img);


// Command Buffer
extern errno_t gdcCreateCommandBuffer(pid_t pid, size_t reqSize, gd_cmdbuf_desc_t* _Nonnull desc);
extern errno_t gdcDestroyCommandBuffer(pid_t pid, int id);
extern void gdcDestroyCommandBuffersOwnedBy(pid_t pid);
extern errno_t gdcSubmitCommandBuffer(pid_t pid, int img_id, int cmds_id);


// CLUT
extern errno_t gdcClut(size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries);
extern errno_t gdcGetClut(size_t idx, size_t count, gd_rgb32_t* _Nonnull entries);
extern errno_t gdcGetClutInfo(gd_clut_info_t* _Nonnull info);


// Display
extern errno_t gdcDisplayMode(const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op);
extern errno_t gdcGetDisplayInfo(int flavor, gd_display_info_ref_t _Nonnull info);
extern errno_t gdcEnumDisplayModes(int index, gd_display_mode_t* _Nonnull pOutMode);
extern void gdcSetDisplayChangeSignal(vcpu_t _Nullable vp, int signo);
extern void gdcSetLightPenEnabled(bool enabled);


// Cursor
extern errno_t gdcSetCursor(const IOHIDCursor* _Nullable cursor);
extern void gdcUpdateCursor(int16_t x, int16_t y, unsigned int flags);

#endif /* _GDC_H */
