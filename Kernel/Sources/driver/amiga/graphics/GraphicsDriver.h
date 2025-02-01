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
#include "Screen.h"
#include "ScreenConfiguration.h"


extern const char* const kFramebufferName;


final_class(GraphicsDriver, Driver);


extern errno_t GraphicsDriver_Create(DriverRef _Nullable parent, GraphicsDriverRef _Nullable * _Nonnull pOutSelf);

// Screens
extern errno_t GraphicsDriver_CreateScreen(GraphicsDriverRef _Nonnull self, const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, Screen* _Nullable * _Nonnull pOutScreen);
extern errno_t GraphicsDriver_DestroyScreen(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr);

extern const ScreenConfiguration* _Nonnull GraphicsDriver_GetScreenConfiguration(GraphicsDriverRef _Nonnull self, Screen* _Nonnull scr);
extern Size GraphicsDriver_GetScreenSize(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr);

extern errno_t GraphicsDriver_LockScreenPixels(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, PixelAccess access, void* _Nonnull plane[8], size_t bytesPerRow[8], size_t* _Nonnull planeCount);
extern errno_t GraphicsDriver_UnlockScreenPixels(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr);

extern errno_t GraphicsDriver_SetCLUTEntry(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, size_t idx, RGBColor32 color);
extern errno_t GraphicsDriver_SetCLUTEntries(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, size_t idx, size_t count, const RGBColor32* _Nonnull entries);


// Sprites
extern errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, int* _Nonnull pOutSpriteId);
extern errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, int spriteId);
extern errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, int spriteId, int x, int y);
extern errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, int spriteId, bool isVisible);


// Display
extern errno_t GraphicsDriver_SetCurrentScreen(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr);
extern Screen* _Nullable GraphicsDriver_GetCurrentScreen(GraphicsDriverRef _Nonnull self);

extern errno_t GraphicsDriver_UpdateDisplay(GraphicsDriverRef _Nonnull self);
extern Size GraphicsDriver_GetDisplaySize(GraphicsDriverRef _Nonnull self);

extern void GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull self, bool enabled);
extern bool GraphicsDriver_GetLightPenPosition(GraphicsDriverRef _Nonnull self, int16_t* _Nonnull pPosX, int16_t* _Nonnull pPosY);


// Mouse Cursor
extern void GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull self, const void* pBitmap, const void* pMask);
extern void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull self, bool isVisible);
extern void GraphicsDriver_SetMouseCursorHiddenUntilMove(GraphicsDriverRef _Nonnull self, bool flag);
extern void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, Point loc);
extern void GraphicsDriver_SetMouseCursorPositionFromInterruptContext(GraphicsDriverRef _Nonnull self, int16_t x, int16_t y);

#endif /* GraphicsDriver_h */
