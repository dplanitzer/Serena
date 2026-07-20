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


// Pixel buffers
extern errno_t AGADriver_CreateBuffer(AGADriverRef _Nonnull self, int width, int height, vio_pixfmt_t pixelFormat, int* _Nonnull pOutId);
extern errno_t AGADriver_DestroyBuffer(AGADriverRef _Nonnull self, int id);
extern errno_t AGADriver_GetBufferInfo(AGADriverRef _Nonnull self, int id, vio_buffer_info_t* _Nonnull pOutInfo);
extern errno_t AGADriver_MapBuffer(AGADriverRef _Nonnull self, int id, int mode, vio_buffer_data_t* _Nonnull pOutMapping);
extern errno_t AGADriver_UnmapBuffer(AGADriverRef _Nonnull self, int id);
extern errno_t AGADriver_BufferCommands(AGADriverRef _Nonnull self, int buf_id, int cmds_id, size_t offset);


// Sprites
extern void AGADriver_GetSpriteCaps(AGADriverRef _Nonnull self, vio_sprite_caps_t* _Nonnull cp);


// Command buffers
extern errno_t AGADriver_CreateCommandBuffer(AGADriverRef _Nonnull self, size_t size, vio_cmdbuf_desc_t* _Nonnull desc);
extern errno_t AGADriver_DestroyCommandBuffer(AGADriverRef _Nonnull self, int id);


// In-kernel command buffer utilities
extern void* _Nonnull gdCmdClut(void* _Nonnull addr, size_t idx, size_t count, const vio_rgb32_t* _Nonnull entries);
extern void* _Nonnull gdCmdDrawPixels(void* _Nonnull addr, int buf_id, const void* _Nonnull planes[], size_t bytesPerRow, vio_pixfmt_t format);
extern void* _Nonnull gdCmdClearPixels(void* _Nonnull addr, int buf_id);
extern void* _Nonnull gdCmdBindSpriteBuffer(void* _Nonnull addr, int target, int buf_id);
extern void* _Nonnull gdCmdSpritePosition(void* _Nonnull addr, int spr_id, int16_t x, int16_t y);
extern void* _Nonnull gdCmdSpriteVisible(void* _Nonnull addr, int spr_id, bool isVisible);
extern void* _Nonnull gdCmdEnd(void* _Nonnull addr);


// Framebuffer
extern errno_t AGADriver_CreateFramebuffer(AGADriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId);
extern errno_t AGADriver_DestroyFramebuffer(AGADriverRef _Nonnull self, int id);
extern errno_t AGADriver_AttachBuffer(AGADriverRef _Nonnull self, int fb_id, int buf_id);
extern errno_t AGADriver_GetFramebufferInfo(AGADriverRef _Nonnull self, int id, vio_clut_info_t* _Nonnull pOutInfo);
extern errno_t AGADriver_SetCurrentFramebuffer(AGADriverRef _Nonnull self, int fb_id);
extern int AGADriver_GetCurrentFramebuffer(AGADriverRef _Nonnull self);


// Display
extern errno_t AGADriver_GetClut(AGADriverRef _Nonnull self, size_t idx, size_t count, vio_rgb32_t* _Nonnull entries);
extern errno_t AGADriver_GetClutInfo(AGADriverRef _Nonnull self, gd_clut_info_t* _Nonnull info);
extern int AGADriver_GetScreenbuffer(AGADriverRef _Nonnull self);
extern errno_t AGADriver_DisplayCommands(AGADriverRef _Nonnull self, int id, size_t offset);

#endif /* AGADriver_h */
