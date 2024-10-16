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
#include "Color.h"
#include "Surface.h"


typedef struct ColorTable {
    size_t                      entryCount;
    const RGBColor32* _Nonnull  entry;
} ColorTable;


struct ScreenConfiguration;
typedef struct ScreenConfiguration ScreenConfiguration;

// The supported screen configurations
extern const ScreenConfiguration kScreenConfig_NTSC_320_200_60;
extern const ScreenConfiguration kScreenConfig_NTSC_640_200_60;
extern const ScreenConfiguration kScreenConfig_NTSC_320_400_30;
extern const ScreenConfiguration kScreenConfig_NTSC_640_400_30;

extern const ScreenConfiguration kScreenConfig_PAL_320_256_50;
extern const ScreenConfiguration kScreenConfig_PAL_640_256_50;
extern const ScreenConfiguration kScreenConfig_PAL_320_512_25;
extern const ScreenConfiguration kScreenConfig_PAL_640_512_25;

extern int ScreenConfiguration_GetPixelWidth(const ScreenConfiguration* pConfig);
extern int ScreenConfiguration_GetPixelHeight(const ScreenConfiguration* pConfig);
extern int ScreenConfiguration_GetRefreshRate(const ScreenConfiguration* pConfig);
extern bool ScreenConfiguration_IsInterlaced(const ScreenConfiguration* pConfig);

extern const char* const kGraphicsDriverName;


final_class(GraphicsDriver, Driver);


typedef int SpriteID;


extern errno_t GraphicsDriver_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, GraphicsDriverRef _Nullable * _Nonnull pOutSelf);

extern const ScreenConfiguration* _Nonnull GraphicsDriver_GetCurrentScreenConfiguration(GraphicsDriverRef _Nonnull self);

// Framebuffer access
extern Size GraphicsDriver_GetFramebufferSize(GraphicsDriverRef _Nonnull self);
extern errno_t GraphicsDriver_LockFramebufferPixels(GraphicsDriverRef _Nonnull self, SurfaceAccess access, void* _Nonnull plane[8], size_t bytesPerRow[8], size_t* _Nonnull planeCount);
extern void GraphicsDriver_UnlockFramebufferPixels(GraphicsDriverRef _Nonnull self);


// CLUT
extern errno_t GraphicsDriver_SetCLUTEntry(GraphicsDriverRef _Nonnull self, int idx, RGBColor32 color);
extern void GraphicsDriver_SetCLUT(GraphicsDriverRef _Nonnull self, const ColorTable* pCLUT);


// Sprites
extern errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull self, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, SpriteID* _Nonnull pOutSpriteId);
extern errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull self, SpriteID spriteId);
extern errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, SpriteID spriteId, int x, int y);
extern errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, SpriteID spriteId, bool isVisible);


// Light Pen
extern errno_t GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull self, bool enabled);
extern bool GraphicsDriver_GetLightPenPosition(GraphicsDriverRef _Nonnull self, int16_t* _Nonnull pPosX, int16_t* _Nonnull pPosY);


// Mouse Cursor
extern void GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull self, const void* pBitmap, const void* pMask);
extern void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull self, bool isVisible);
extern void GraphicsDriver_SetMouseCursorHiddenUntilMouseMoves(GraphicsDriverRef _Nonnull self, bool flag);
extern void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, Point loc);
extern void GraphicsDriver_SetMouseCursorPositionFromInterruptContext(GraphicsDriverRef _Nonnull self, int16_t x, int16_t y);

#endif /* GraphicsDriver_h */
