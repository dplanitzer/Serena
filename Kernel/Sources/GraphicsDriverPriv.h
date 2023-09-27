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
#include "Lock.h"
#include "Platform.h"
#include "Semaphore.h"


#define MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION   5
struct _ScreenConfiguration {
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
// Copper Program
//

typedef struct _CopperProgram {
    CopperInstruction   entry[1];
} CopperProgram;


//
// Copper Scheduler
//

#define COPF_CONTEXT_SWITCH_REQ (1 << 7)
#define COPF_INTERLACED         (1 << 6)
typedef struct _CopperScheduler {
    const CopperProgram* _Nullable  readyOddFieldProg;
    const CopperProgram* _Nullable  readyEvenFieldProg;

    const CopperProgram* _Nullable  runningOddFieldProg;
    const CopperProgram* _Nullable  runningEvenFieldProg;

    UInt32                          flags;
} CopperScheduler;

extern void CopperScheduler_Init(CopperScheduler* _Nonnull pScheduler);
extern void CopperScheduler_Deinit(CopperScheduler* _Nonnull pScheduler);
extern void CopperScheduler_ScheduleProgram(CopperScheduler* _Nonnull pScheduler, const CopperProgram* _Nullable pOddFieldProg, const CopperProgram* _Nullable pEvenFieldProg);
extern void CopperScheduler_Run(CopperScheduler* _Nonnull pScheduler);


//
// Sprite
//

#define NUM_HARDWARE_SPRITES    8
#define MAX_SPRITE_WIDTH        16
#define MAX_SPRITE_HEIGHT       511

typedef struct _Sprite {
    UInt16*     data;   // sprxctl, sprxctl, (plane0, plane1), ..., 0, 0
    UInt16      height;
} Sprite;


//
// Screen
//

typedef struct _Screen {
    Surface* _Nullable                  framebuffer;            // the screen framebuffer
    const ScreenConfiguration* _Nonnull screenConfig;
    PixelFormat                         pixelFormat;
    Sprite* _Nonnull                    nullSprite;
    Sprite* _Nonnull                    sprite[NUM_HARDWARE_SPRITES];
    Bool                                isInterlaced;
} Screen;


//
// Copper Compiler
//

extern Int CopperCompiler_GetScreenRefreshProgramInstructionCount(Screen* _Nonnull pScreen);
extern void CopperCompiler_CompileScreenRefreshProgram(CopperInstruction* _Nonnull pCode, Screen* _Nonnull pScreen, Bool isLightPenEnabled, Bool isOddField);
extern ErrorCode CopperProgram_CreateScreenRefresh(Screen* _Nonnull pScreen, Bool isLightPenEnabled, Bool isOddField, CopperProgram* _Nullable * _Nonnull pOutProg);
extern void CopperProgram_Destroy(CopperProgram* _Nullable pProg);


//
// Graphics Driver
//

typedef struct _GraphicsDriver {
    Screen* _Nonnull        screen;
    Sprite* _Nonnull        nullSprite;
    Lock                    lock;   // protects the driver and the current screen
    CopperScheduler         copperScheduler;
    InterruptHandlerID      vb_irq_handler;
    Semaphore               vblank_sema;
    Bool                    isLightPenEnabled;  // Applies to all screens
} GraphicsDriver;


extern void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull pDriver);
extern void GraphicsDriver_StopVideoRefresh_Locked(GraphicsDriverRef _Nonnull pDriver);

extern ErrorCode GraphicsDriver_SetCurrentScreen_Locked(GraphicsDriverRef _Nonnull pDriver, Screen* _Nonnull pScreen);

extern void GraphicsDriver_SetCLUT(GraphicsDriverRef _Nonnull pDriver, const ColorTable* pCLUT);

#endif /* GraphicsDriverPriv_h */
