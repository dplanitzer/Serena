//
//  GraphicsDriver.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriver.h"
#include "InterruptController.h"
#include "Platform.h"
#include "Semaphore.h"

#define PACK_U16(_15, _14, _13, _12, _11, _10, _9, _8, _7, _6, _5, _4, _3, _2, _1, _0) \
    (UInt16)(((_15) << 15) | ((_14) << 14) | ((_13) << 13) | ((_12) << 12) | ((_11) << 11) |\
             ((_10) << 10) |  ((_9) <<  9) |  ((_8) <<  8) |  ((_7) <<  7) |  ((_6) <<  6) |\
              ((_5) <<  5) |  ((_4) <<  4) |  ((_3) <<  3) |  ((_2) <<  2) |  ((_1) <<  1) | (_0))

#define _ 0
#define o 1

#define ARROW_BITS_PLANE1 \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,_,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,_,_,_,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ),
static const UInt16 gArrow_Plane1[] = {
    ARROW_BITS_PLANE1
};

#define ARROW_BITS_PLANE0 \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,o,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,o,o,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,_,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,_,_,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ),
static const UInt16 gArrow_Plane0[] = {
    ARROW_BITS_PLANE0
};


// DDIWSTART = specific to mode. See hardware reference manual
// DDIWSTOP = last 8 bits of pixel position
// DDFSTART = low res: DDIWSTART / 2 - 8; high res: DDIWSTART / 2 - 4
// DDFSTOP = low res: DDFSTART + 8*(nwords - 2); high res: DDFSTART + 4*(nwords - 2)
const VideoConfiguration kVideoConfig_NTSC_320_200_60 = {0, 320, 200, 60,
    0x81, 0x2c, 0xc1, 0xf4, 0x38, 0xd0, 0, 0x0200, 0x00,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const VideoConfiguration kVideoConfig_NTSC_640_200_60 = {1, 640, 200, 60,
    0x81, 0x2c, 0xc1, 0xf4, 0x3c, 0xd4, 0, 0x8200, 0x10,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};
const VideoConfiguration kVideoConfig_NTSC_320_400_30 = {2, 320, 400, 30,
    0x81, 0x2c, 0xc1, 0xf4, 0x38, 0xd0, 40, 0x0204, 0x01,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const VideoConfiguration kVideoConfig_NTSC_640_400_30 = {3, 640, 400, 30,
    0x81, 0x2c, 0xc1, 0xf4, 0x3c, 0xd4, 80, 0x8204, 0x11,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};

const VideoConfiguration kVideoConfig_PAL_320_256_50 = {4, 320, 256, 50,
    0x81, 0x2c, 0xc1, 0x2c, 0x38, 0xd0, 0, 0x0200, 0x00,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const VideoConfiguration kVideoConfig_PAL_640_256_50 = {5, 640, 256, 50,
    0x81, 0x2c, 0xc1, 0x2c, 0x3c, 0xd4, 0, 0x8200, 0x10,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};
const VideoConfiguration kVideoConfig_PAL_320_512_25 = {6, 320, 512, 25,
    0x81, 0x2c, 0xc1, 0x2c, 0x38, 0xd0, 40, 0x0204, 0x01,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const VideoConfiguration kVideoConfig_PAL_640_512_25 = {7, 640, 512, 25,
    0x81, 0x2c, 0xc1, 0x2c, 0x3c, 0xd4, 80, 0x8204, 0x11,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};


////////////////////////////////////////////////////////////////////////////////
// MARK:
// MARK: Copper program compiler
////////////////////////////////////////////////////////////////////////////////


static const UInt8 BPLxPTH[MAX_PLANE_COUNT] = {BPL1PTH, BPL2PTH, BPL3PTH, BPL4PTH, BPL5PTH, BPL6PTH};
static const UInt8 BPLxPTL[MAX_PLANE_COUNT] = {BPL1PTL, BPL2PTL, BPL3PTL, BPL4PTL, BPL5PTL, BPL6PTL};

// Computes the size of a Copper program. The size is given in terms of the
// number of Copper instruction words.
static Int CopperCompiler_GetScreenRefreshProgramInstructionCount(const VideoConfiguration* _Nonnull pConfig, Int nplanes)
{
    return 3                // BPLCONx
            + 2             // DIWSTART, DIWSTOP
            + 2             // DDFSTART, DDF_STOP
            + 2             // BPLxMOD
            + 2 * nplanes   // BPLxPT[nplanes]
            + 16            // SPRxPT
            + 1;            // DMACON
}

// Compiles a screen refresh Copper program into the given buffer (which must be
// big enough to store the program).
static void CopperCompiler_CompileScreenRefreshProgram(CopperInstruction* _Nonnull pCode, const VideoConfiguration* _Nonnull pConfig, Surface* _Nonnull pSurface, Bool isOddField, const UInt16* _Nonnull pNullSprite, const UInt16* _Nonnull pSprite, Bool isLightPenEnabled)
{
    const UInt32 firstLineByteOffset = isOddField ? 0 : pConfig->ddf_mod;
    const UInt16 lpen_mask = isLightPenEnabled ? 0x0008 : 0x0000;
    Int ip = 0;
    
    // BPLCONx
    pCode[ip++] = COP_MOVE(BPLCON0, pConfig->bplcon0 | lpen_mask | ((UInt16)pSurface->planeCount & 0x07) << 12);
    pCode[ip++] = COP_MOVE(BPLCON1, 0);
    pCode[ip++] = COP_MOVE(BPLCON2, 0x0024);
    
    // DIWSTART / DIWSTOP
    pCode[ip++] = COP_MOVE(DIWSTART, (pConfig->diw_start_v << 8) | pConfig->diw_start_h);
    pCode[ip++] = COP_MOVE(DIWSTOP, (pConfig->diw_stop_v << 8) | pConfig->diw_stop_h);
    
    // DDFSTART / DDFSTOP
    pCode[ip++] = COP_MOVE(DDFSTART, pConfig->ddf_start);
    pCode[ip++] = COP_MOVE(DDFSTOP, pConfig->ddf_stop);
    
    // BPLxMOD
    pCode[ip++] = COP_MOVE(BPL1MOD, pConfig->ddf_mod);
    pCode[ip++] = COP_MOVE(BPL2MOD, pConfig->ddf_mod);
    
    // BPLxPT
    for (Int i = 0; i < pSurface->planeCount; i++) {
        const UInt32 bplpt = (UInt32)(pSurface->planes[i]) + firstLineByteOffset;
        
        pCode[ip++] = COP_MOVE(BPLxPTH[i], (bplpt >> 16) & UINT16_MAX);
        pCode[ip++] = COP_MOVE(BPLxPTL[i], bplpt & UINT16_MAX);
    }

    // SPRxPT
    const UInt32 spr32 = (UInt32)pSprite;
    const UInt16 sprH = (spr32 >> 16) & UINT16_MAX;
    const UInt16 sprL = spr32 & UINT16_MAX;
    const UInt32 nullspr = (const UInt32)pNullSprite;
    const UInt16 nullsprH = (nullspr >> 16) & UINT16_MAX;
    const UInt16 nullsprL = nullspr & UINT16_MAX;

    pCode[ip++] = COP_MOVE(SPR0PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR0PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR1PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR1PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR2PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR2PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR3PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR3PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR4PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR4PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR5PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR5PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR6PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR6PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR7PTH, sprH);
    pCode[ip++] = COP_MOVE(SPR7PTL, sprL);
    
    // DMACON
    pCode[ip++] = COP_MOVE(DMACON, 0x8300);
    // XXX turned the mouse cursor off for now
//    pCode[ip++] = COP_MOVE(DMACON, 0x8320);
}

// Compiles a Copper program to display a non-interlaced screen or a single field
// of an interlaced screen.
static ErrorCode CopperProgram_CreateScreenRefresh(const VideoConfiguration* _Nonnull pConfig, Surface* _Nonnull pSurface, Bool isOddField, const UInt16* _Nonnull pNullSprite, const UInt16* _Nonnull pSprite, Bool isLightPenEnabled, CopperInstruction* _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    Int ip = 0;
    const Int nFrameInstructions = CopperCompiler_GetScreenRefreshProgramInstructionCount(pConfig, pSurface->planeCount);
    const Int nInstructions = nFrameInstructions + 1;
    CopperInstruction* pCode;
    
    try(kalloc_options(nInstructions * sizeof(CopperInstruction), KALLOC_OPTION_UNIFIED, (Byte**) &pCode));
    
    CopperCompiler_CompileScreenRefreshProgram(&pCode[ip], pConfig, pSurface, isOddField, pNullSprite, pSprite, isLightPenEnabled);
    ip += nFrameInstructions;

    // end instructions
    pCode[ip++] = COP_END();
    
    *pOutProg = pCode;
    return EOK;
    
catch:
    return err;
}

// Frees the given Copper program.
static void CopperProgram_Destroy(CopperInstruction* _Nullable pCode)
{
    kfree((Byte*)pCode);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Display
////////////////////////////////////////////////////////////////////////////////


typedef struct _Display {
    Surface* _Nullable                  surface;                // the display framebuffer
    CopperInstruction* _Nullable        copperProgramOddField;  // Odd field interlaced or non-interlaced
    CopperInstruction* _Nullable        copperProgramEvenField; // Even field interlaced or NULL
    const VideoConfiguration* _Nonnull  videoConfig;
    PixelFormat                         pixelFormat;
    const UInt16* _Nonnull              nullSprite;
    const UInt16* _Nonnull              mouseCursorSprite;
    Bool                                isInterlaced;
    Bool                                isLightPenEnabled;
} Display;

static ErrorCode Display_CompileCopperPrograms(Display* _Nonnull pDisplay);


static void Display_Destroy(Display* _Nullable pDisplay)
{
    if (pDisplay) {
        CopperProgram_Destroy(pDisplay->copperProgramEvenField);
        pDisplay->copperProgramEvenField = NULL;
        
        CopperProgram_Destroy(pDisplay->copperProgramOddField);
        pDisplay->copperProgramOddField = NULL;
        
        Surface_UnlockPixels(pDisplay->surface);
        Surface_Destroy(pDisplay->surface);
        pDisplay->surface = NULL;
        
        kfree((Byte*)pDisplay);
    }
}

// Creates a display object.
// \param pConfig the video configuration
// \param pixelFormat the pixel format (must be supported by the config)
// \return the display or null
static ErrorCode Display_Create(const VideoConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, const UInt16* _Nonnull pNullSprite, const UInt16* _Nonnull pSprite, Bool isLightPenEnabled, Display* _Nullable * _Nonnull pOutDisplay)
{
    decl_try_err();
    Display* pDisplay;
    
    try(kalloc_cleared(sizeof(Display), (Byte**) &pDisplay));
    
    pDisplay->videoConfig = pConfig;
    pDisplay->pixelFormat = pixelFormat;
    pDisplay->nullSprite = pNullSprite;
    pDisplay->mouseCursorSprite = pSprite;
    pDisplay->isInterlaced = (pConfig->bplcon0 & BPLCON0_LACE) != 0;
    pDisplay->isLightPenEnabled = isLightPenEnabled;

    
    // Allocate an appropriate framebuffer
    try(Surface_Create(pConfig->width, pConfig->height, pixelFormat, &pDisplay->surface));
    
    
    // Lock the new surface
    try(Surface_LockPixels(pDisplay->surface, kSurfaceAccess_Read|kSurfaceAccess_Write));
    
    
    // Compile the Copper program
    try(Display_CompileCopperPrograms(pDisplay));
    
    *pOutDisplay = pDisplay;
    return EOK;
    
catch:
    Display_Destroy(pDisplay);
    *pOutDisplay = NULL;
    return err;
}

static ErrorCode Display_CompileCopperPrograms(Display* _Nonnull pDisplay)
{
    decl_try_err();
    
    if (pDisplay->isInterlaced) {
        try(CopperProgram_CreateScreenRefresh(pDisplay->videoConfig, pDisplay->surface, true, pDisplay->nullSprite, pDisplay->mouseCursorSprite, pDisplay->isLightPenEnabled, &pDisplay->copperProgramOddField));
        try(CopperProgram_CreateScreenRefresh(pDisplay->videoConfig, pDisplay->surface, false, pDisplay->nullSprite, pDisplay->mouseCursorSprite, pDisplay->isLightPenEnabled, &pDisplay->copperProgramEvenField));
    } else {
        try(CopperProgram_CreateScreenRefresh(pDisplay->videoConfig, pDisplay->surface, true, pDisplay->nullSprite, pDisplay->mouseCursorSprite, pDisplay->isLightPenEnabled, &pDisplay->copperProgramOddField));
        pDisplay->copperProgramEvenField = NULL;
    }

    return EOK;

catch:
    return err;
}

static ErrorCode Display_SetLightPenEnabled(Display* _Nonnull pDisplay, Bool enabled)
{
    decl_try_err();

    pDisplay->isLightPenEnabled = enabled;
    try(Display_CompileCopperPrograms(pDisplay));
    copper_schedule_program(pDisplay->copperProgramOddField, pDisplay->copperProgramEvenField, 0);
    return EOK;

catch:
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: GraphicsDriver
////////////////////////////////////////////////////////////////////////////////


typedef struct _GraphicsDriver {
    Display* _Nonnull       display;
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


static ErrorCode GraphicsDriver_SetVideoConfiguration(GraphicsDriverRef _Nonnull pDriver, const VideoConfiguration* _Nonnull pConfig, PixelFormat pixelFormat);


// Creates a graphics driver instance with a framebuffer based on the given video
// configuration and pixel format.
ErrorCode GraphicsDriver_Create(const VideoConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, GraphicsDriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    GraphicsDriver* pDriver;
    
    try(kalloc_cleared(sizeof(GraphicsDriver), (Byte**) &pDriver));

    
    // Initialize sprites
    try(kalloc_options(sizeof(UInt16)*2, KALLOC_OPTION_UNIFIED, (Byte**) &pDriver->sprite_null));
    pDriver->sprite_null[0] = 0;
    pDriver->sprite_null[1] = 0;

    pDriver->mouse_cursor_width = 16;
    pDriver->mouse_cursor_height = 16;
    pDriver->mouse_cursor_hotspot_x = 1;
    pDriver->mouse_cursor_hotspot_y = 1;

    const Int sprsiz = (pDriver->mouse_cursor_height + 2)*2;
    Int i, j;
    try(kalloc_options(sizeof(UInt16) * sprsiz, KALLOC_OPTION_UNIFIED, (Byte**) &pDriver->sprite_mouse));
    for (i = 0, j = 0; i < sprsiz; i += 2, j++) {
        pDriver->sprite_mouse[i + 0] = gArrow_Plane0[j];
        pDriver->sprite_mouse[i + 1] = gArrow_Plane1[j];
    }
    pDriver->sprite_mouse[0] = (pConfig->diw_start_v << 8) | (pConfig->diw_start_h >> 1);
    pDriver->sprite_mouse[1] = (pConfig->diw_start_v + pDriver->mouse_cursor_height) << 8;
    
    // Initialize vblank tools
    Semaphore_Init(&pDriver->vblank_sema, 0);
    try(InterruptController_AddSemaphoreInterruptHandler(gInterruptController,
                                                         INTERRUPT_ID_VERTICAL_BLANK,
                                                         INTERRUPT_HANDLER_PRIORITY_HIGHEST,
                                                         &pDriver->vblank_sema,
                                                         &pDriver->vb_irq_handler));
    
    // Initialize the video config related stuff
    // XXX Set the console colors
    denise_set_clut_entry(0, 0x0000);
    denise_set_clut_entry(1, 0x00f0);
    for (int i = 2; i < 32; i++) {
        denise_set_clut_entry(i, 0x0fff);
    }
    denise_set_clut_entry(29, 0x0fff);
    denise_set_clut_entry(30, 0x0000);
    denise_set_clut_entry(31, 0x0000);

    InterruptController_SetInterruptHandlerEnabled(gInterruptController, pDriver->vb_irq_handler, true);

    try(GraphicsDriver_SetVideoConfiguration(pDriver, pConfig, pixelFormat));
//    try(GraphicsDriver_SetVideoConfiguration(pDriver, &kVideoConfig_NTSC_320_200_60 /*pConfig*/, pixelFormat));
//    try(GraphicsDriver_SetVideoConfiguration(pDriver, &kVideoConfig_PAL_640_512_25 /*pConfig*/, pixelFormat));

    *pOutDriver = pDriver;
    return EOK;

catch:
    GraphicsDriver_Destroy(pDriver);
    *pOutDriver = NULL;
    return err;
}

// Deallocates the given graphics driver.
void GraphicsDriver_Destroy(GraphicsDriverRef _Nullable pDriver)
{
    if (pDriver) {
        denise_set_clut_entry(0, 0);
        denise_stop_video_refresh();
        
        try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, pDriver->vb_irq_handler));
        pDriver->vb_irq_handler = 0;
        
        Semaphore_Deinit(&pDriver->vblank_sema);
        
        Display_Destroy(pDriver->display);
        pDriver->display = NULL;

        kfree((Byte*)pDriver->sprite_mouse);
        pDriver->sprite_mouse = NULL;
        
        kfree((Byte*)pDriver->sprite_null);
        pDriver->sprite_null = NULL;
        
        kfree((Byte*)pDriver);
    }
}

Size GraphicsDriver_GetFramebufferSize(GraphicsDriverRef _Nonnull pDriver)
{
    Surface* pFramebuffer = GraphicsDriver_GetFramebuffer(pDriver);
    
    return (pFramebuffer) ? Surface_GetPixelSize(pFramebuffer) : Size_Zero;
}

// Returns a reference to the currently active framebuffer. NULL is returned if
// no framebuffer is active which implies that the video signal generator is
// turned off.
// \return the framebuffer or NULL
Surface* _Nullable GraphicsDriver_GetFramebuffer(GraphicsDriverRef _Nonnull pDriver)
{
    return pDriver->display->surface;
}

// Waits for a vblank to occur. This function acts as a vblank barrier meaning
// that it will wait for some vblank to happen after this function has been invoked.
// No vblank that occured before this function was called will make it return.
static ErrorCode GraphicsDriver_WaitForVerticalBlank(GraphicsDriverRef _Nonnull pDriver)
{
    decl_try_err();

    // First purge the vblank sema to ensure that we don't accidentaly pick up some
    // vblank that has happened before this function has been called. Then wait
    // for the actual vblank.
    try(Semaphore_TryAcquire(&pDriver->vblank_sema));
    try(Semaphore_Acquire(&pDriver->vblank_sema, kTimeInterval_Infinity));
    return EOK;

catch:
    return err;
}

// Changes the video configuration. The driver allocates an appropriate framebuffer
// and activates the video refresh hardware.
// \param pConfig the video configuration
// \param pixelFormat the pixel format (must be supported by the config)
// \return the error code
static ErrorCode GraphicsDriver_SetVideoConfiguration(GraphicsDriverRef _Nonnull pDriver, const VideoConfiguration* _Nonnull pConfig, PixelFormat pixelFormat)
{
    decl_try_err();
    Display* pOldDisplay = pDriver->display;

    // Allocate a new display
    Display* pNewDisplay;
    try(Display_Create(pConfig, pixelFormat, pDriver->sprite_null, pDriver->sprite_mouse, pDriver->is_light_pen_enabled, &pNewDisplay));
    
    
    // Update the graphics device state.
    pDriver->display = pNewDisplay;
    
    // Turn video refresh back on and point it to the new copper program
    copper_schedule_program(pNewDisplay->copperProgramOddField, pNewDisplay->copperProgramEvenField, 0);
    
    // Wait for the vblank. Once we got a vblank we know that the DMA is no longer
    // accessing the old framebuffer
    try(GraphicsDriver_WaitForVerticalBlank(pDriver));
    
    
    // Free the old display
    Display_Destroy(pOldDisplay);

    return EOK;

catch:
    return err;
}

// Fills the framebuffer with the background color. This is black for RGB direct
// pixel formats and index 0 for RGB indexed pixel formats.
void GraphicsDriver_Clear(GraphicsDriverRef _Nonnull pDriver)
{
    const Surface* pSurface = GraphicsDriver_GetFramebuffer(pDriver);

    if (pSurface != NULL) {
        const Int nbytes = pSurface->bytesPerRow * pSurface->height;
    
        for (Int i = 0; i < pSurface->planeCount; i++) {
            Bytes_ClearRange(pSurface->planes[i], nbytes);
        }
    }
}

// Fills the pixels in the given rectangular framebuffer area with the given color.
void GraphicsDriver_FillRect(GraphicsDriverRef _Nonnull pDriver, Rect rect, Color color)
{
    const Surface* pSurface = GraphicsDriver_GetFramebuffer(pDriver);
    if (pSurface == NULL) {
        return;
    }

    const Rect bounds = Rect_Make(0, 0, pSurface->width, pSurface->height);
    const Rect r = Rect_Intersection(rect, bounds);
    
    if (Rect_IsEmpty(r)) {
        return;
    }
    
    assert(color.tag == kColorType_Index);
    
    for (Int i = 0; i < pSurface->planeCount; i++) {
        const Bool bit = (color.u.index & (1 << i)) ? true : false;
        
        for (Int y = r.y; y < r.y + r.height; y++) {
            const BitPointer pBits = BitPointer_Make(pSurface->planes[i] + y * pSurface->bytesPerRow, r.x);
            
            if (bit) {
                Bits_SetRange(pBits, r.width);
            } else {
                Bits_ClearRange(pBits, r.width);
            }
        }
    }
}

// Copies the given rectangular framebuffer area to a different location in the framebuffer.
// Parts of the source rectangle which are outside the bounds of the framebuffer are treated as
// transparent. This means that the corresponding destination pixels will be left alone and not
// overwritten.
void GraphicsDriver_CopyRect(GraphicsDriverRef _Nonnull pDriver, Rect srcRect, Point dstLoc)
{
    if (srcRect.width == 0 || srcRect.height == 0 || (srcRect.x == dstLoc.x && srcRect.y == dstLoc.y)) {
        return;
    }
    
    const Surface* pSurface = GraphicsDriver_GetFramebuffer(pDriver);
    if (pSurface == NULL) {
        return;
    }

    const Rect src_r = srcRect;
    const Rect dst_r = Rect_Make(dstLoc.x, dstLoc.y, src_r.width, src_r.height);
    const Int fb_width = pSurface->width;
    const Int fb_height = pSurface->height;
    const Int bytesPerRow = pSurface->bytesPerRow;
    const Int src_end_y = src_r.y + src_r.height - 1;
    const Int dst_clipped_left_span = (dst_r.x < 0) ? -dst_r.x : 0;
    const Int dst_clipped_right_span = max(dst_r.x + dst_r.width - fb_width, 0);
    const Int dst_x = max(dst_r.x, 0);
    const Int src_x = src_r.x + dst_clipped_left_span;
    const Int dst_width = max(dst_r.width - dst_clipped_left_span - dst_clipped_right_span, 0);

    for (Int i = 0; i < pSurface->planeCount; i++) {
        Byte* pPlane = pSurface->planes[i];

        if (dst_r.y >= src_r.y && dst_r.y <= src_end_y) {
            const Int dst_clipped_y_span = max(dst_r.y + dst_r.height - fb_height, 0);
            const Int dst_y_min = max(dst_r.y, 0);
            Int src_y = src_r.y + src_r.height - 1;
            Int dst_y = dst_r.y + dst_r.height - dst_clipped_y_span - 1;
            
            while (dst_y >= dst_y_min) {
                Bits_CopyRange(BitPointer_Make(pPlane + dst_y * bytesPerRow, dst_x),
                               BitPointer_Make(pPlane + src_y * bytesPerRow, src_x),
                               dst_width);
                src_y--; dst_y--;
            }
        }
        else {
            const Int dst_clipped_y_span = (dst_r.y < 0) ? -dst_r.y : 0;
            Int dst_y = max(dst_r.y, 0);
            const Int dst_y_max = min(dst_r.y + dst_r.height, fb_height);
            Int src_y = src_r.y + dst_clipped_y_span;
            
            while (dst_y < dst_y_max) {
                Bits_CopyRange(BitPointer_Make(pPlane + dst_y * bytesPerRow, dst_x),
                               BitPointer_Make(pPlane + src_y * bytesPerRow, src_x),
                               dst_width);
                src_y++; dst_y++;
            }
        }
    }
}

// Blits a monochromatic 8x8 pixel glyph to the given position in the framebuffer.
void GraphicsDriver_BlitGlyph_8x8bw(GraphicsDriverRef _Nonnull pDriver, const Byte* _Nonnull pGlyphBitmap, Int x, Int y)
{
    const Surface* pSurface = GraphicsDriver_GetFramebuffer(pDriver);
    if (pSurface == NULL) {
        return;
    }

    const Int bytesPerRow = pSurface->bytesPerRow;
    const Int maxX = pSurface->width >> 3;
    const Int maxY = pSurface->height >> 3;
    
    if (x < 0 || y < 0 || x >= maxX || y >= maxY) {
        return;
    }
    
    Byte* dst = pSurface->planes[0] + (y << 3) * bytesPerRow + x;
    const Byte* src = pGlyphBitmap;
    
    *dst = *src++; dst += bytesPerRow;      // 0
    *dst = *src++; dst += bytesPerRow;      // 1
    *dst = *src++; dst += bytesPerRow;      // 2
    *dst = *src++; dst += bytesPerRow;      // 3
    *dst = *src++; dst += bytesPerRow;      // 4
    *dst = *src++; dst += bytesPerRow;      // 5
    *dst = *src++; dst += bytesPerRow;      // 6
    *dst = *src;                            // 7
}

// Enables / disables the h/v raster position latching triggered by a light pen.
ErrorCode GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull pDriver, Bool enabled)
{
    if (pDriver->is_light_pen_enabled != enabled) {
        pDriver->is_light_pen_enabled = enabled;
        return Display_SetLightPenEnabled(pDriver->display, enabled);
    } else {
        return EOK;
    }
}

// Returns the current position of the light pen if the light pen triggered.
Bool GraphicsDriver_GetLightPenPosition(GraphicsDriverRef _Nonnull pDriver, Int16* _Nonnull pPosX, Int16* _Nonnull pPosY)
{
    register volatile Int32* p_vposr = (Int32*)(CUSTOM_BASE + VPOSR);
    
    // Read VHPOSR first time
    register Int32 posr0 = *p_vposr;
    
    // XXX wait for scanline microseconds
    for (int i = 0; i < 1000; i++) {}
    
    // Read VHPOSR a second time
    register Int32 posr1 = *p_vposr;
    
    
    // Check whether the light pen triggered
    // See Amiga Reference Hardware Manual p233.
    if (posr0 == posr1) {
        if ((posr0 & 0x0000ffff) < 0x10500) {
            *pPosX = (posr0 & 0x000000ff) << 1;
            *pPosY = (posr0 & 0x1ff00) >> 8;
            
            if (pDriver->display->isInterlaced && posr0 < 0) {
                // long frame (odd field) is offset in Y by one
                *pPosY += 1;
            }
            return true;
        }
    }

    return false;
}

// Returns the size of the current mouse cursor.
Size GraphicsDriver_GetMouseCursorSize(GraphicsDriverRef _Nonnull pDriver)
{
    const UInt16 hshift = (pDriver->display->videoConfig->spr_shift & 0xf0) >> 4;
    const UInt16 vshift = pDriver->display->videoConfig->spr_shift & 0x0f;

    return Size_Make(pDriver->mouse_cursor_width << hshift, pDriver->mouse_cursor_height << vshift);
}

// Returns the offset of the hot spot inside the mouse cursor relative to the
// mouse cursor origin (top-left corner of the mouse cursor image).
Point GraphicsDriver_GetMouseCursorHotSpot(GraphicsDriverRef _Nonnull pDriver)
{
    const UInt16 hshift = (pDriver->display->videoConfig->spr_shift & 0xf0) >> 4;
    const UInt16 vshift = pDriver->display->videoConfig->spr_shift & 0x0f;
    
    return Point_Make(pDriver->mouse_cursor_hotspot_x << hshift, pDriver->mouse_cursor_hotspot_y << vshift);
}

// Shows or hides the mouse cursor. The cursor is immediately shown or hidden.
void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull pDriver, Bool isVisible)
{
    // XXX not yet
}

// Moves the mouse cursor image to the given position in framebuffer coordinates.
// The update is executed immediately.
void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull pDriver, Int16 xPos, Int16 yPos)
{
    const UInt16 hshift = (pDriver->display->videoConfig->spr_shift & 0xf0) >> 4;
    const UInt16 vshift = pDriver->display->videoConfig->spr_shift & 0x0f;
    const UInt16 hstart = (xPos >> hshift) + pDriver->display->videoConfig->diw_start_h;
    const UInt16 vstart = (yPos >> vshift) + pDriver->display->videoConfig->diw_start_v;
    const UInt16 vstop = vstart + pDriver->mouse_cursor_height;
    const UInt16 sprxpos = ((vstart & 0xff) << 8) | ((hstart & 0x1fe) >> 1);
    const UInt16 sprxctl = ((vstop & 0xff) << 8) | (((vstart >> 8) & 0x01) << 2) | (((vstop >> 8) & 0x01) << 1) | (hstart & 0x001);

    pDriver->sprite_mouse[0] = sprxpos;
    pDriver->sprite_mouse[1] = sprxctl;
}
