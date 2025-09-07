//
//  GraphicsDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef GraphicsDriver_h
#define GraphicsDriver_h

#include <driver/Driver.h>
#include <kpi/fb.h>
#include <sched/vcpu.h>


final_class(GraphicsDriver, Driver);


extern errno_t GraphicsDriver_Create(GraphicsDriverRef _Nullable * _Nonnull pOutSelf);

// Surfaces
extern errno_t GraphicsDriver_CreateSurface(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int* _Nonnull pOutId);
extern errno_t GraphicsDriver_DestroySurface(GraphicsDriverRef _Nonnull self, int id);

extern errno_t GraphicsDriver_GetSurfaceInfo(GraphicsDriverRef _Nonnull self, int id, SurfaceInfo* _Nonnull pOutInfo);

extern errno_t GraphicsDriver_MapSurface(GraphicsDriverRef _Nonnull self, int id, MapPixels mode, SurfaceMapping* _Nonnull pOutMapping);
extern errno_t GraphicsDriver_UnmapSurface(GraphicsDriverRef _Nonnull self, int id);


// CLUT
extern errno_t GraphicsDriver_CreateCLUT(GraphicsDriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId);
extern errno_t GraphicsDriver_DestroyCLUT(GraphicsDriverRef _Nonnull self, int id);
extern errno_t GraphicsDriver_GetCLUTInfo(GraphicsDriverRef _Nonnull self, int id, CLUTInfo* _Nonnull pOutInfo);
extern errno_t GraphicsDriver_SetCLUTEntries(GraphicsDriverRef _Nonnull self, int id, size_t idx, size_t count, const RGBColor32* _Nonnull entries);


// Sprites
extern errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutSpriteId);
extern errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull self, int spriteId);
extern errno_t GraphicsDriver_SetSpritePixels(GraphicsDriverRef _Nonnull self, int spriteId, const uint16_t* _Nonnull pPlanes[2]);
extern errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, int spriteId, int x, int y);
extern errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, int spriteId, bool isVisible);


// Screens
extern errno_t GraphicsDriver_SetScreenConfig(GraphicsDriverRef _Nonnull self, const int* _Nullable config);
extern errno_t GraphicsDriver_GetScreenConfig(GraphicsDriverRef _Nonnull self, int* _Nonnull config, size_t bufsiz);

extern errno_t GraphicsDriver_SetScreenCLUTEntries(GraphicsDriverRef _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries);
extern void GraphicsDriver_GetScreenSize(GraphicsDriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);


// Light Pen
extern void GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull self, bool enabled);


// Mouse Cursor

// Acquires and activates the mouse cursor. The mouse cursor is initially transparent
// and thus invisible. You assign an image to the mouse cursor by calling the
// SetMouseCursor() function. Returns EOK on success and EBUSY if the display
// driver implements the mouser cursor with the help of a sprite and the sprite
// is currently acquired by someone else.
extern errno_t GraphicsDriver_AcquireMouseCursor(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat);

// Relinquishes the mouse cursor and makes the underlying sprite available for
// other uses again.
extern void GraphicsDriver_RelinquishMouseCursor(GraphicsDriverRef _Nonnull self);

// Sets the mouse cursor image to the pixels 'planes'.
extern errno_t GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull self, const uint16_t* _Nullable planes[2]);

// Sets the position of the mouse cursor. Note that the mouse cursor is only
// visible as long as at least some part of it is inside the visible display
// area. Additionally this API guarantees that the mouse cursor will be hidden
// if either 'x' or 'y' is == INT_MIN
extern void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, int x, int y);

extern void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull self, bool isVisible);

extern void GraphicsDriver_SetScreenConfigObserver(GraphicsDriverRef _Nonnull self, vcpu_t _Nullable vp, int signo);

#endif /* GraphicsDriver_h */
