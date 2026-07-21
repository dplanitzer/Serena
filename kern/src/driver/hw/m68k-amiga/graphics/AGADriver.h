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
extern errno_t AGADriver_CreateBuffer(AGADriverRef _Nonnull self, int width, int height, gd_pixfmt_t pixelFormat, int* _Nonnull pOutId);
extern errno_t AGADriver_DestroyBuffer(AGADriverRef _Nonnull self, int id);
extern errno_t AGADriver_GetBufferInfo(AGADriverRef _Nonnull self, int id, gd_buffer_info_t* _Nonnull pOutInfo);
extern errno_t AGADriver_MapBuffer(AGADriverRef _Nonnull self, int id, int mode, gd_buffer_data_t* _Nonnull pOutMapping);
extern errno_t AGADriver_UnmapBuffer(AGADriverRef _Nonnull self, int id);


// Sprites
extern void AGADriver_GetSpriteCaps(AGADriverRef _Nonnull self, gd_sprite_caps_t* _Nonnull cp);


// Command buffers
extern errno_t AGADriver_CreateCommandBuffer(AGADriverRef _Nonnull self, size_t size, gd_cmdbuf_desc_t* _Nonnull desc);
extern errno_t AGADriver_DestroyCommandBuffer(AGADriverRef _Nonnull self, int id);
extern errno_t AGADriver_SubmitCommandBuffer(AGADriverRef _Nonnull self, int queue_id, int cmds_id);


// In-kernel command buffer utilities
extern void* _Nonnull gdCmdClut(void* _Nonnull addr, size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries);
extern void* _Nonnull gdCmdWritePixels(void* _Nonnull addr, int buf_id, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format);
extern void* _Nonnull gdCmdClearPixels(void* _Nonnull addr, int buf_id);
extern void* _Nonnull gdCmdBindSpriteBuffer(void* _Nonnull addr, int target, int buf_id);
extern void* _Nonnull gdCmdSpritePosition(void* _Nonnull addr, int spr_id, int16_t x, int16_t y);
extern void* _Nonnull gdCmdSpriteVisible(void* _Nonnull addr, int spr_id, bool isVisible);
extern void* _Nonnull gdCmdEnd(void* _Nonnull addr);


// CLUT
extern errno_t AGADriver_GetClut(AGADriverRef _Nonnull self, size_t idx, size_t count, gd_rgb32_t* _Nonnull entries);
extern errno_t AGADriver_GetClutInfo(AGADriverRef _Nonnull self, gd_clut_info_t* _Nonnull info);


// Display
extern errno_t AGADriver_DisplayMode(AGADriverRef _Nonnull self, const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op);
extern errno_t AGADriver_GetDisplayInfo(AGADriverRef _Nonnull self, int flavor, gd_display_info_ref_t _Nonnull info);
extern errno_t AGADriver_EnumDisplayModes(AGADriverRef _Nonnull self, int index, gd_display_mode_t* _Nonnull pOutMode);

#endif /* AGADriver_h */
