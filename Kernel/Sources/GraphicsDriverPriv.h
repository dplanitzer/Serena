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


//
// Screen
//

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


//
// Copper Compiler
//

extern Int CopperCompiler_GetScreenRefreshProgramInstructionCount(Screen* _Nonnull pScreen);
extern void CopperCompiler_CompileScreenRefreshProgram(CopperInstruction* _Nonnull pCode, Screen* _Nonnull pScreen, Bool isOddField);
extern ErrorCode CopperProgram_CreateScreenRefresh(Screen* _Nonnull pScreen, Bool isOddField, CopperInstruction* _Nullable * _Nonnull pOutProg);
extern void CopperProgram_Destroy(CopperInstruction* _Nullable pCode);


//
// Copper Scheduler
//

#define COPF_CONTEXT_SWITCH_REQ (1 << 7)
#define COPF_INTERLACED         (1 << 6)
typedef struct _CopperScheduler {
    const CopperInstruction* _Nullable  readyOddFieldProg;
    const CopperInstruction* _Nullable  readyEvenFieldProg;

    const CopperInstruction* _Nullable  runningOddFieldProg;
    const CopperInstruction* _Nullable  runningEvenFieldProg;

    UInt32                              flags;
} CopperScheduler;

extern void CopperScheduler_Init(CopperScheduler* _Nonnull pScheduler);
extern void CopperScheduler_Deinit(CopperScheduler* _Nonnull pScheduler);
extern void CopperScheduler_ScheduleProgram(CopperScheduler* _Nonnull pScheduler, const CopperInstruction* _Nullable pOddFieldProg, const CopperInstruction* _Nullable pEvenFieldProg);
extern void CopperScheduler_ContextSwitch(CopperScheduler* _Nonnull pScheduler);


//
// Graphics Driver
//

typedef struct _GraphicsDriver {
    Screen* _Nonnull        screen;
    CopperScheduler         copperScheduler;
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


extern void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull pDriver);
extern void GraphicsDriver_StopVideoRefresh(GraphicsDriverRef _Nonnull pDriver);

extern void GraphicsDriver_SetCLUT(GraphicsDriverRef _Nonnull pDriver, const ColorTable* pCLUT);

#endif /* GraphicsDriverPriv_h */
