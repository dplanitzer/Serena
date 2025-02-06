//
//  GraphicsDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef GraphicsDriver_h
#define GraphicsDriver_h

#include <klib/klib.h>
#include <driver/Driver.h>
#include <System/Framebuffer.h>
#include "Screen.h"
#include "VideoConfiguration.h"


extern const char* const kFramebufferName;


final_class(GraphicsDriver, Driver);


extern errno_t GraphicsDriver_Create(DriverRef _Nullable parent, GraphicsDriverRef _Nullable * _Nonnull pOutSelf);

// Surfaces
extern errno_t GraphicsDriver_CreateSurface(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int* _Nonnull pOutId);
extern errno_t GraphicsDriver_DestroySurface(GraphicsDriverRef _Nonnull self, int id);

extern errno_t GraphicsDriver_GetSurfaceInfo(GraphicsDriverRef _Nonnull self, int id, SurfaceInfo* _Nonnull pOutInfo);

extern errno_t GraphicsDriver_MapSurface(GraphicsDriverRef _Nonnull self, int id, MapPixels mode, SurfaceMapping* _Nonnull pOutMapping);
extern errno_t GraphicsDriver_UnmapSurface(GraphicsDriverRef _Nonnull self, int id);


// Screens
extern errno_t GraphicsDriver_CreateScreen(GraphicsDriverRef _Nonnull self, const VideoConfiguration* _Nonnull vidCfg, int surfaceId, int* _Nonnull pOutId);
extern errno_t GraphicsDriver_DestroyScreen(GraphicsDriverRef _Nonnull self, int id);

extern errno_t GraphicsDriver_GetVideoConfiguration(GraphicsDriverRef _Nonnull self, int id, VideoConfiguration* _Nonnull pOutVidConfig);

extern errno_t GraphicsDriver_SetCLUTEntry(GraphicsDriverRef _Nonnull self, int id, size_t idx, RGBColor32 color);
extern errno_t GraphicsDriver_SetCLUTEntries(GraphicsDriverRef _Nonnull self, int id, size_t idx, size_t count, const RGBColor32* _Nonnull entries);


// Sprites
extern errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull self, int screenId, int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutSpriteId);
extern errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull self, int spriteId);
extern errno_t GraphicsDriver_SetSpritePixels(GraphicsDriverRef _Nonnull self, int spriteId, const uint16_t* _Nonnull pPlanes[2]);
extern errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, int spriteId, int x, int y);
extern errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, int spriteId, bool isVisible);


// Display
extern errno_t GraphicsDriver_SetCurrentScreen(GraphicsDriverRef _Nonnull self, int screenId);
extern int GraphicsDriver_GetCurrentScreen(GraphicsDriverRef _Nonnull self);

extern errno_t GraphicsDriver_UpdateDisplay(GraphicsDriverRef _Nonnull self);
extern void GraphicsDriver_GetDisplaySize(GraphicsDriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);


// Light Pen
extern void GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull self, bool enabled);
extern bool GraphicsDriver_GetLightPenPositionFromInterruptContext(GraphicsDriverRef _Nonnull self, int16_t* _Nonnull pPosX, int16_t* _Nonnull pPosY);


// Mouse Cursor
extern errno_t GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull self, const uint16_t* _Nullable planes[2], int width, int height, PixelFormat pixelFormat);
extern void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, int x, int y);
extern void GraphicsDriver_SetMouseCursorPositionFromInterruptContext(GraphicsDriverRef _Nonnull self, int x, int y);


// Introspection
extern errno_t GraphicsDriver_GetVideoConfigurationRange(GraphicsDriverRef _Nonnull self, VideoConfigurationRange* _Nonnull config, size_t bufSize, size_t* _Nonnull pIter);

#endif /* GraphicsDriver_h */
