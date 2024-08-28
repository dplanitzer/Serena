//
//  GraphicsDriverPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef GraphicsDriverPriv_h
#define GraphicsDriverPriv_h

#include "GraphicsDriver.h"
#include <dispatcher/Lock.h>
#include <dispatcher/Semaphore.h>
#include <dispatchqueue/DispatchQueue.h>
#include <hal/Platform.h>
#include "InterruptController.h"
#include "MousePainter.h"


#define MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION   5
struct ScreenConfiguration {
    int16_t       uniqueId;
    int16_t       width;
    int16_t       height;
    int8_t        fps;
    uint8_t       diw_start_h;        // display window start
    uint8_t       diw_start_v;
    uint8_t       diw_stop_h;         // display window stop
    uint8_t       diw_stop_v;
    uint8_t       ddf_start;          // data fetch start
    uint8_t       ddf_stop;           // data fetch stop
    uint8_t       ddf_mod;            // number of padding bytes stored in memory between scan lines
    uint16_t      bplcon0;            // BPLCON0 template value
    uint8_t       spr_shift;          // Shift factors that should be applied to X & Y coordinates to convert them from screen coords to sprite coords [h:4,v:4]
    int8_t        pixelFormatCount;   // Number of supported pixel formats
    PixelFormat pixelFormat[MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION];
};


//
// Copper Program
//

typedef struct CopperProgram {
    SListNode           node;
    CopperInstruction   entry[1];
} CopperProgram;


//
// Copper Scheduler
//

#define COPF_CONTEXT_SWITCH_REQ (1 << 7)
#define COPF_INTERLACED         (1 << 6)
typedef struct CopperScheduler {
    const CopperProgram* _Nullable  readyOddFieldProg;
    const CopperProgram* _Nullable  readyEvenFieldProg;

    const CopperProgram* _Nullable  runningOddFieldProg;
    const CopperProgram* _Nullable  runningEvenFieldProg;

    uint32_t                        flags;

    Semaphore                       retirementSignaler;
    SList                           retiredProgs;
    DispatchQueueRef _Nonnull       retiredProgsCollector;
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

typedef struct Sprite {
    uint16_t*     data;   // sprxctl, sprxctl, (plane0, plane1)..., 0, 0
    int16_t       x;
    int16_t       y;
    uint16_t      height;
    bool        isVisible;
} Sprite;


//
// Screen
//

typedef struct Screen {
    Surface* _Nullable                  framebuffer;        // the screen framebuffer
    const ScreenConfiguration* _Nonnull screenConfig;
    PixelFormat                         pixelFormat;
    Sprite* _Nonnull                    nullSprite;
    Sprite* _Nonnull                    sprite[NUM_HARDWARE_SPRITES];
    bool                                isInterlaced;
    int16_t                             clutCapacity;       // how many entries the physical CLUT supports for this screen configuration
} Screen;

#define MAX_CLUT_ENTRIES    32


//
// Copper Compiler
//

extern int CopperCompiler_GetScreenRefreshProgramInstructionCount(Screen* _Nonnull pScreen);
extern CopperInstruction* _Nonnull CopperCompiler_CompileScreenRefreshProgram(CopperInstruction* _Nonnull pCode, Screen* _Nonnull pScreen, bool isLightPenEnabled, bool isOddField);
extern errno_t CopperProgram_CreateScreenRefresh(Screen* _Nonnull pScreen, bool isLightPenEnabled, bool isOddField, CopperProgram* _Nullable * _Nonnull pOutProg);
extern void CopperProgram_Destroy(CopperProgram* _Nullable pProg);


//
// Graphics Driver
//

final_class_ivars(GraphicsDriver, Object,
    Screen* _Nonnull    screen;
    Sprite* _Nonnull    nullSprite;
    Lock                lock;   // protects the driver and the current screen
    CopperScheduler     copperScheduler;
    InterruptHandlerID  vb_irq_handler;
    Semaphore           vblank_sema;
    bool                isLightPenEnabled;  // Applies to all screens
    MousePainter        mousePainter;
);


extern void _GraphicsDriver_Deinit(GraphicsDriverRef _Nonnull pDriver);

extern void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull pDriver);
extern void GraphicsDriver_StopVideoRefresh_Locked(GraphicsDriverRef _Nonnull pDriver);

extern errno_t GraphicsDriver_SetCurrentScreen_Locked(GraphicsDriverRef _Nonnull pDriver, Screen* _Nonnull pScreen);

#endif /* GraphicsDriverPriv_h */
