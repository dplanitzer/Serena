//
//  GraphicsDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef GraphicsDriver_h
#define GraphicsDriver_h

#include <driver/DisplayDriver.h>


final_class(GraphicsDriver, DisplayDriver);


extern errno_t GraphicsDriver_Create(GraphicsDriverRef _Nullable * _Nonnull pOutSelf);

// Surfaces
extern errno_t GraphicsDriver_CreateSurface2d(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int* _Nonnull pOutId);
extern errno_t GraphicsDriver_DestroySurface(GraphicsDriverRef _Nonnull self, int id);

extern errno_t GraphicsDriver_GetSurfaceInfo(GraphicsDriverRef _Nonnull self, int id, SurfaceInfo* _Nonnull pOutInfo);

extern errno_t GraphicsDriver_MapSurface(GraphicsDriverRef _Nonnull self, int id, MapPixels mode, SurfaceMapping* _Nonnull pOutMapping);
extern errno_t GraphicsDriver_UnmapSurface(GraphicsDriverRef _Nonnull self, int id);

extern errno_t GraphicsDriver_WritePixels(GraphicsDriverRef _Nonnull self, int id, const void* _Nonnull planes[], size_t bytesPerRow, PixelFormat format);
extern errno_t GraphicsDriver_ClearPixels(GraphicsDriverRef _Nonnull self, int id);

extern errno_t GraphicsDriver_BindSurface(GraphicsDriverRef _Nonnull self, int target, int id);


// CLUT
extern errno_t GraphicsDriver_CreateCLUT(GraphicsDriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId);
extern errno_t GraphicsDriver_DestroyCLUT(GraphicsDriverRef _Nonnull self, int id);
extern errno_t GraphicsDriver_GetCLUTInfo(GraphicsDriverRef _Nonnull self, int id, CLUTInfo* _Nonnull pOutInfo);
extern errno_t GraphicsDriver_SetCLUTEntries(GraphicsDriverRef _Nonnull self, int id, size_t idx, size_t count, const RGBColor32* _Nonnull entries);


// Sprites
extern void GraphicsDriver_GetSpriteCaps(GraphicsDriverRef _Nonnull self, SpriteCaps* _Nonnull cp);
extern errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, int spriteId, int x, int y);
extern errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, int spriteId, bool isVisible);


// Screens
extern errno_t GraphicsDriver_SetScreenConfig(GraphicsDriverRef _Nonnull self, const intptr_t* _Nullable conf);
extern errno_t GraphicsDriver_GetScreenConfig(GraphicsDriverRef _Nonnull self, intptr_t* _Nonnull conf, size_t bufsiz);

extern errno_t GraphicsDriver_SetScreenCLUTEntries(GraphicsDriverRef _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries);

#endif /* GraphicsDriver_h */
