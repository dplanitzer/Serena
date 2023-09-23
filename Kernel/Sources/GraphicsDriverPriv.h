//
//  GraphicsDriverPriv.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef GraphicsDriverPriv_h
#define GraphicsDriverPriv_h

#include "GraphicsDriver.h"
#include "InterruptController.h"
#include "Platform.h"
#include "Semaphore.h"


#define MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION   5
struct _VideoConfiguration {
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
};


#define CLUT_ENTRY_COUNT 32
typedef struct _ColorTable {
    UInt16  entry[CLUT_ENTRY_COUNT];
} ColorTable;


typedef struct _Screen {
    Surface* _Nullable                  framebuffer;            // the screen framebuffer
    CopperInstruction* _Nullable        copperProgramOddField;  // Odd field interlaced or non-interlaced
    CopperInstruction* _Nullable        copperProgramEvenField; // Even field interlaced or NULL
    const VideoConfiguration* _Nonnull  videoConfig;
    PixelFormat                         pixelFormat;
    const UInt16* _Nonnull              nullSprite;
    const UInt16* _Nonnull              mouseCursorSprite;
    Bool                                isInterlaced;
    Bool                                isLightPenEnabled;
} Screen;


typedef struct _GraphicsDriver {
    Screen* _Nonnull        screen;
    InterruptHandlerID      vb_irq_handler;
    Semaphore               vblank_sema;
    UInt16* _Nonnull        sprite_null;
    UInt16* _Nonnull        sprite_mouse;
    Int16                   mouse_cursor_width;
    Int16                   mouse_cursor_height;
    Int16                   mouse_cursor_hotspot_x;
    Int16                   mouse_cursor_hotspot_y;
    Bool                    is_light_pen_enabled;
} GraphicsDriver;


extern void GraphicsDriver_StopVideoRefresh(GraphicsDriverRef _Nonnull pDriver);

extern void GraphicsDriver_SetCLUTEntry(GraphicsDriverRef _Nonnull pDriver, Int idx, const RGBColor* _Nonnull pColor);
extern void GraphicsDriver_SetCLUT(GraphicsDriverRef _Nonnull pDriver, const ColorTable* pCLUT);

#endif /* GraphicsDriverPriv_h */
