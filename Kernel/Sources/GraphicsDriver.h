//
//  GraphicsDriver.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef GraphicsDriver_h
#define GraphicsDriver_h

#include <klib/klib.h>
#include "IOResource.h"
#include "Surface.h"


typedef enum _ColorType {
    kColorType_RGB,
    kColorType_Index
} ColorType;


// A RGB color
typedef struct _RGBColor {
    UInt8   r, g, b;
} RGBColor;


// A color type
typedef struct _Color {
    ColorType   tag;
    union {
        RGBColor    rgb;
        Int         index;
    }           u;
} Color;


static inline RGBColor RGBColor_Make(Int r, Int g, Int b)
{
    RGBColor clr;
    
    clr.r = (UInt8)r;
    clr.g = (UInt8)g;
    clr.b = (UInt8)b;
    return clr;
}

static inline Color Color_MakeRGB(Int r, Int g, Int b)
{
    Color clr;
    
    clr.tag = kColorType_RGB;
    clr.u.rgb.r = (UInt8)r;
    clr.u.rgb.g = (UInt8)g;
    clr.u.rgb.b = (UInt8)b;
    return clr;
}

static inline Color Color_MakeIndex(Int idx)
{
    Color clr;
    
    clr.tag = kColorType_Index;
    clr.u.index = idx;
    return clr;
}


struct _ScreenConfiguration;
typedef struct _ScreenConfiguration ScreenConfiguration;

// The supported screen configurations
extern const ScreenConfiguration kScreenConfig_NTSC_320_200_60;
extern const ScreenConfiguration kScreenConfig_NTSC_640_200_60;
extern const ScreenConfiguration kScreenConfig_NTSC_320_400_30;
extern const ScreenConfiguration kScreenConfig_NTSC_640_400_30;

extern const ScreenConfiguration kScreenConfig_PAL_320_256_50;
extern const ScreenConfiguration kScreenConfig_PAL_640_256_50;
extern const ScreenConfiguration kScreenConfig_PAL_320_512_25;
extern const ScreenConfiguration kScreenConfig_PAL_640_512_25;

extern Int ScreenConfiguration_GetPixelWidth(const ScreenConfiguration* pConfig);
extern Int ScreenConfiguration_GetPixelHeight(const ScreenConfiguration* pConfig);
extern Int ScreenConfiguration_GetRefreshRate(const ScreenConfiguration* pConfig);
extern Bool ScreenConfiguration_IsInterlaced(const ScreenConfiguration* pConfig);


OPAQUE_CLASS(GraphicsDriver, IOResource);
typedef struct _GraphicsDriverMethodTable {
    IOResourceMethodTable   super;
} GraphicsDriverMethodTable;


typedef Int SpriteID;


extern ErrorCode GraphicsDriver_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, GraphicsDriverRef _Nullable * _Nonnull pOutDriver);
extern void GraphicsDriver_Destroy(GraphicsDriverRef _Nullable pDriver);

extern const ScreenConfiguration* _Nonnull GraphicsDriver_GetCurrentScreenConfiguration(GraphicsDriverRef _Nonnull pDriver);

extern Surface* _Nullable GraphicsDriver_GetFramebuffer(GraphicsDriverRef _Nonnull pDriver);
extern Size GraphicsDriver_GetFramebufferSize(GraphicsDriverRef _Nonnull pDriver);

extern ErrorCode GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull pDriver, Bool enabled);
extern Bool GraphicsDriver_GetLightPenPosition(GraphicsDriverRef _Nonnull pDriver, Int16* _Nonnull pPosX, Int16* _Nonnull pPosY);


// Sprites
extern ErrorCode GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull pDriver, const UInt16* _Nonnull pPlanes[2], Int x, Int y, Int width, Int height, Int priority, SpriteID* _Nonnull pOutSpriteId);
extern ErrorCode GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull pDriver, SpriteID spriteId);
extern ErrorCode GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull pDriver, SpriteID spriteId, Int x, Int y);
extern ErrorCode GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull pDriver, SpriteID spriteId, Bool isVisible);


// Mouse Cursor
extern void GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull pDriver, const Byte* pBitmap, const Byte* pMask);
extern void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull pDriver, Bool isVisible);
extern void GraphicsDriver_SetMouseCursorHiddenUntilMouseMoves(GraphicsDriverRef _Nonnull pDriver, Bool flag);
extern void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull pDriver, Point loc);
extern void GraphicsDriver_SetMouseCursorPositionFromInterruptContext(GraphicsDriverRef _Nonnull pDriver, Int16 x, Int16 y);


// Drawing
extern void GraphicsDriver_SetCLUTEntry(GraphicsDriverRef _Nonnull pDriver, Int idx, const RGBColor* _Nonnull pColor);

extern void GraphicsDriver_Clear(GraphicsDriverRef _Nonnull pDriver);
extern void GraphicsDriver_FillRect(GraphicsDriverRef _Nonnull pDriver, Rect rect, Color color);
extern void GraphicsDriver_CopyRect(GraphicsDriverRef _Nonnull pDriver, Rect srcRect, Point dstLoc);
extern void GraphicsDriver_BlitGlyph_8x8bw(GraphicsDriverRef _Nonnull pDriver, const Byte* _Nonnull pGlyphBitmap, Int x, Int y);

#endif /* GraphicsDriver_h */
