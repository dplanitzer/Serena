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
extern errno_t AGADriver_CreateSurface2d(AGADriverRef _Nonnull self, int width, int height, pixfmt_t pixelFormat, int* _Nonnull pOutId);
extern errno_t AGADriver_DestroySurface(AGADriverRef _Nonnull self, int id);

extern errno_t AGADriver_GetSurfaceInfo(AGADriverRef _Nonnull self, int id, surface_info_t* _Nonnull pOutInfo);

extern errno_t AGADriver_MapSurface(AGADriverRef _Nonnull self, int id, int mode, surface_mapping_t* _Nonnull pOutMapping);
extern errno_t AGADriver_UnmapSurface(AGADriverRef _Nonnull self, int id);

extern errno_t AGADriver_WritePixels(AGADriverRef _Nonnull self, int id, const void* _Nonnull planes[], size_t bytesPerRow, pixfmt_t format);
extern errno_t AGADriver_ClearPixels(AGADriverRef _Nonnull self, int id);

extern errno_t AGADriver_BindSurface(AGADriverRef _Nonnull self, int target, int id);


// CLUT
extern errno_t AGADriver_CreateCLUT(AGADriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId);
extern errno_t AGADriver_DestroyCLUT(AGADriverRef _Nonnull self, int id);
extern errno_t AGADriver_GetCLUTInfo(AGADriverRef _Nonnull self, int id, clut_info_t* _Nonnull pOutInfo);
extern errno_t AGADriver_SetCLUTEntries(AGADriverRef _Nonnull self, int id, size_t idx, size_t count, const color_rgb32_t* _Nonnull entries);


// Sprites
extern void AGADriver_GetSpriteCaps(AGADriverRef _Nonnull self, sprite_caps_t* _Nonnull cp);
extern errno_t AGADriver_SetSpritePosition(AGADriverRef _Nonnull self, int spriteId, int x, int y);
extern errno_t AGADriver_SetSpriteVisible(AGADriverRef _Nonnull self, int spriteId, bool isVisible);


// Screens
extern errno_t AGADriver_SetScreenConfig(AGADriverRef _Nonnull self, const intptr_t* _Nullable conf);
extern errno_t AGADriver_GetScreenConfig(AGADriverRef _Nonnull self, intptr_t* _Nonnull conf, size_t bufsiz);

extern errno_t AGADriver_SetScreenCLUTEntries(AGADriverRef _Nonnull self, size_t idx, size_t count, const color_rgb32_t* _Nonnull entries);

#endif /* AGADriver_h */
