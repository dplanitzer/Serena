//
//  AGADriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef AGADriver_h
#define AGADriver_h

#include <driver/IODriver.h>
#include <kpi/framebuffer.h>


final_class(AGADriver, IODriver);


extern errno_t AGADriver_Create(AGADriverRef _Nullable * _Nonnull pOutSelf);

// Surfaces
extern errno_t AGADriver_CreateBuffer(AGADriverRef _Nonnull self, int width, int height, vio_pixfmt_t pixelFormat, int* _Nonnull pOutId);
extern errno_t AGADriver_DestroyBuffer(AGADriverRef _Nonnull self, int id);

extern errno_t AGADriver_GetBufferInfo(AGADriverRef _Nonnull self, int id, vio_buffer_info_t* _Nonnull pOutInfo);

extern errno_t AGADriver_MapBuffer(AGADriverRef _Nonnull self, int id, int mode, vio_buffer_data_t* _Nonnull pOutMapping);
extern errno_t AGADriver_UnmapBuffer(AGADriverRef _Nonnull self, int id);


// Framebuffer
extern errno_t AGADriver_CreateFramebuffer(AGADriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId);
extern errno_t AGADriver_DestroyFramebuffer(AGADriverRef _Nonnull self, int id);
extern errno_t AGADriver_GetFramebufferInfo(AGADriverRef _Nonnull self, int id, vio_clut_info_t* _Nonnull pOutInfo);


// Sprites
extern void AGADriver_GetSpriteCaps(AGADriverRef _Nonnull self, vio_sprite_caps_t* _Nonnull cp);


// Screens
extern errno_t AGADriver_SetScreenConfig(AGADriverRef _Nonnull self, const intptr_t* _Nullable conf);
extern errno_t AGADriver_GetScreenConfig(AGADriverRef _Nonnull self, intptr_t* _Nonnull conf, size_t bufsiz);


// Command buffers
extern errno_t AGADriver_CreateCommandBuffer(AGADriverRef _Nonnull self, size_t size, vio_cmdbuf_desc_t* _Nonnull desc);
extern errno_t AGADriver_DestroyCommandBuffer(AGADriverRef _Nonnull self, int id);
extern errno_t AGADriver_ExecuteCommandBuffer(AGADriverRef _Nonnull self, int id, size_t offset);


// In-kernel command buffer utilities
extern void* _Nonnull vio_set_clut_rgb32(void* _Nonnull addr, int clut_id, size_t idx, size_t count, const vio_rgb32_t* _Nonnull entries);
extern void* _Nonnull vio_write_pixels(void* _Nonnull addr, int buf_id, const void* _Nonnull planes[], size_t bytesPerRow, vio_pixfmt_t format);
extern void* _Nonnull vio_clear_pixels(void* _Nonnull addr, int buf_id);
extern void* _Nonnull vio_bind_buffer(void* _Nonnull addr, int target, int buf_id);
extern void* _Nonnull vio_put_sprite(void* _Nonnull addr, int spr_id, int16_t x, int16_t y);
extern void* _Nonnull vio_show_sprite(void* _Nonnull addr, int spr_id, bool isVisible);
extern void* _Nonnull vio_end(void* _Nonnull addr);

#endif /* AGADriver_h */
