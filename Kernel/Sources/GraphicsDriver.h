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
#include "Geometry.h"
#include "Surface.h"


typedef enum _ColorType {
    kColorType_RGB,
    kColorType_Index
} ColorType;


// A RGB color
typedef struct _RGBColor {
    Int8    r, g, b;
} RGBColor;


// A color type
typedef struct _Color {
    ColorType   tag;
    union {
        RGBColor    rgb;
        Int         index;
    }           u;
} Color;


static inline Color Color_MakeRGB(Int r, Int g, Int b)
{
    Color clr;
    
    clr.tag = kColorType_RGB;
    clr.u.rgb.r = (Int8)r;
    clr.u.rgb.g = (Int8)g;
    clr.u.rgb.b = (Int8)b;
    return clr;
}

static inline Color Color_MakeIndex(Int idx)
{
    Color clr;
    
    clr.tag = kColorType_Index;
    clr.u.index = idx;
    return clr;
}


#define MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION   5
typedef struct _VideoConfiguration {
    Int16       uniqueId;
    Int16       width;
    Int16       height;
    Int8        fps;
    UInt8       diw_start_h;        // display window start
    UInt8       diw_start_v;
    UInt8       diw_stop_h;         // display window stop
    UInt8       diw_stop_v;
    UInt8       ddf_start;          // data fetch start
    UInt8       ddf_stop;           // data fetch stop
    UInt8       ddf_mod;            // number of padding bytes stored in memory between scan lines
    UInt16      bplcon0;            // BPLCON0 template value
    UInt8       spr_shift;          // Shift factors that should be applied to X & Y coordinates to convert them from screen coords to sprite coords [h:4,v:4]
    Int8        pixelFormatCount;   // Number of supported pixel formats
    PixelFormat pixelFormat[MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION];
} VideoConfiguration;


// The supported video configurations
extern const VideoConfiguration kVideoConfig_NTSC_320_200_60;
extern const VideoConfiguration kVideoConfig_NTSC_640_200_60;
extern const VideoConfiguration kVideoConfig_NTSC_320_400_30;
extern const VideoConfiguration kVideoConfig_NTSC_640_400_30;

extern const VideoConfiguration kVideoConfig_PAL_320_256_50;
extern const VideoConfiguration kVideoConfig_PAL_640_256_50;
extern const VideoConfiguration kVideoConfig_PAL_320_512_25;
extern const VideoConfiguration kVideoConfig_PAL_640_512_25;


struct _GraphicsDriver;
typedef struct _GraphicsDriver* GraphicsDriverRef;


extern ErrorCode GraphicsDriver_Create(const VideoConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, GraphicsDriverRef _Nullable * _Nonnull pOutDriver);
extern void GraphicsDriver_Destroy(GraphicsDriverRef _Nullable pDriver);

extern Surface* _Nullable GraphicsDriver_GetFramebuffer(GraphicsDriverRef _Nonnull pDriver);
extern Size GraphicsDriver_GetFramebufferSize(GraphicsDriverRef _Nonnull pDriver);

extern void GraphicsDriver_Clear(GraphicsDriverRef _Nonnull pDriver);
extern void GraphicsDriver_FillRect(GraphicsDriverRef _Nonnull pDriver, Rect rect, Color color);
extern void GraphicsDriver_CopyRect(GraphicsDriverRef _Nonnull pDriver, Rect srcRect, Point dstLoc);
extern void GraphicsDriver_BlitGlyph_8x8bw(GraphicsDriverRef _Nonnull pDriver, const Byte* _Nonnull pGlyphBitmap, Int x, Int y);

extern ErrorCode GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull pDriver, Bool enabled);
extern Bool GraphicsDriver_GetLightPenPosition(GraphicsDriverRef _Nonnull pDriver, Int16* _Nonnull pPosX, Int16* _Nonnull pPosY);

extern Size GraphicsDriver_GetMouseCursorSize(GraphicsDriverRef _Nonnull pDriver);
extern Point GraphicsDriver_GetMouseCursorHotSpot(GraphicsDriverRef _Nonnull pDriver);
extern void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull pDriver, Bool isVisible);
extern void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull pDriver, Int16 xPos, Int16 yPos);

#endif /* GraphicsDriver_h */
