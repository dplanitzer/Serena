//
//  GraphicsDriver.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Screen Configuration
////////////////////////////////////////////////////////////////////////////////

// DDIWSTART = specific to mode. See hardware reference manual
// DDIWSTOP = last 8 bits of pixel position
// DDFSTART = low res: DDIWSTART / 2 - 8; high res: DDIWSTART / 2 - 4
// DDFSTOP = low res: DDFSTART + 8*(nwords - 2); high res: DDFSTART + 4*(nwords - 2)
const ScreenConfiguration kScreenConfig_NTSC_320_200_60 = {0, 320, 200, 60,
    0x81, 0x2c, 0xc1, 0xf4, 0x38, 0xd0, 0, 0x0200, 0x00,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const ScreenConfiguration kScreenConfig_NTSC_640_200_60 = {1, 640, 200, 60,
    0x81, 0x2c, 0xc1, 0xf4, 0x3c, 0xd4, 0, 0x8200, 0x10,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};
const ScreenConfiguration kScreenConfig_NTSC_320_400_30 = {2, 320, 400, 30,
    0x81, 0x2c, 0xc1, 0xf4, 0x38, 0xd0, 40, 0x0204, 0x01,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const ScreenConfiguration kScreenConfig_NTSC_640_400_30 = {3, 640, 400, 30,
    0x81, 0x2c, 0xc1, 0xf4, 0x3c, 0xd4, 80, 0x8204, 0x11,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};

const ScreenConfiguration kScreenConfig_PAL_320_256_50 = {4, 320, 256, 50,
    0x81, 0x2c, 0xc1, 0x2c, 0x38, 0xd0, 0, 0x0200, 0x00,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const ScreenConfiguration kScreenConfig_PAL_640_256_50 = {5, 640, 256, 50,
    0x81, 0x2c, 0xc1, 0x2c, 0x3c, 0xd4, 0, 0x8200, 0x10,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};
const ScreenConfiguration kScreenConfig_PAL_320_512_25 = {6, 320, 512, 25,
    0x81, 0x2c, 0xc1, 0x2c, 0x38, 0xd0, 40, 0x0204, 0x01,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const ScreenConfiguration kScreenConfig_PAL_640_512_25 = {7, 640, 512, 25,
    0x81, 0x2c, 0xc1, 0x2c, 0x3c, 0xd4, 80, 0x8204, 0x11,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};


Int ScreenConfiguration_GetPixelWidth(const ScreenConfiguration* pConfig)
{
    return pConfig->width;
}

Int ScreenConfiguration_GetPixelHeight(const ScreenConfiguration* pConfig)
{
    return pConfig->height;
}

Int ScreenConfiguration_GetRefreshRate(const ScreenConfiguration* pConfig)
{
    return pConfig->fps;
}

Bool ScreenConfiguration_IsInterlaced(const ScreenConfiguration* pConfig)
{
    return (pConfig->bplcon0 & BPLCON0_LACE) != 0;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Screen
////////////////////////////////////////////////////////////////////////////////

static void Screen_Destroy(Screen* _Nullable pScreen)
{
    if (pScreen) {
        Surface_UnlockPixels(pScreen->framebuffer);
        Surface_Destroy(pScreen->framebuffer);
        pScreen->framebuffer = NULL;
        
        kfree((Byte*)pScreen);
    }
}

// Creates a screen object.
// \param pConfig the video configuration
// \param pixelFormat the pixel format (must be supported by the config)
// \return the screen or null
static ErrorCode Screen_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, UInt16* _Nonnull pNullSprite, Screen* _Nullable * _Nonnull pOutScreen)
{
    decl_try_err();
    Screen* pScreen;
    
    try(kalloc_cleared(sizeof(Screen), (Byte**) &pScreen));
    
    pScreen->screenConfig = pConfig;
    pScreen->pixelFormat = pixelFormat;
    pScreen->nullSprite = pNullSprite;
    pScreen->isInterlaced = ScreenConfiguration_IsInterlaced(pConfig);

    
    // Allocate an appropriate framebuffer
    try(Surface_Create(pConfig->width, pConfig->height, pixelFormat, &pScreen->framebuffer));
    
    
    // Lock the new surface
    try(Surface_LockPixels(pScreen->framebuffer, kSurfaceAccess_Read|kSurfaceAccess_Write));
    
    *pOutScreen = pScreen;
    return EOK;
    
catch:
    Screen_Destroy(pScreen);
    *pOutScreen = NULL;
    return err;
}

static ErrorCode Screen_AcquireSprite(Screen* _Nonnull pScreen, const UInt16* _Nonnull pPlanes[2], Int x, Int y, Int width, Int height, Int priority, SpriteID* _Nonnull pOutSpriteId)
{
    decl_try_err();
    const ScreenConfiguration* pConfig = pScreen->screenConfig;
    UInt16* pSprite;

    if (pScreen->sprite[priority]) {
        throw(EBUSY);
    }

    const Int nWords = 2 + 2*height + 2;
    try(kalloc_options(sizeof(UInt16) * nWords, KALLOC_OPTION_UNIFIED, (Byte**) &pSprite));
    UInt16* sp = pSprite;

    const UInt16 hshift = (pConfig->spr_shift & 0xf0) >> 4;
    const UInt16 vshift = pConfig->spr_shift & 0x0f;
    const UInt16 hstart = pConfig->diw_start_h - 1 + (x >> hshift);
    const UInt16 vstart = pConfig->diw_start_v + (y >> vshift);
    const UInt16 vstop = vstart + (UInt16)height;
    const UInt16 sprxpos = ((vstart & 0x00ff) << 8) | ((hstart & 0x01fe) >> 1);
    const UInt16 sprxctl = ((vstop & 0x00ff) << 8) | (((vstart >> 8) & 0x0001) << 2) | (((vstop >> 8) & 0x0001) << 1) | (hstart & 0x0001);

    *sp++ = sprxpos;
    *sp++ = sprxctl;
    for (Int i = 0; i < height; i++) {
        *sp++ = pPlanes[0][i];
        *sp++ = pPlanes[1][i];
    }
    *sp++ = 0;
    *sp   = 0;

    pScreen->sprite[priority] = pSprite;
    *pOutSpriteId = priority;

    return EOK;

catch:
    *pOutSpriteId = -1;
    return err;
}

// Relinquishes a hardware sprite
static void Screen_RelinquishSprite(Screen* _Nonnull pScreen, SpriteID spriteId)
{
    if (spriteId >= 0) {
        // XXX actually free the old sprite instead of leaking it
        pScreen->sprite[spriteId] = pScreen->nullSprite;
    }
}

// Updates the position of a hardware sprite
static void Screen_SetSpritePosition(Screen* _Nonnull pScreen, SpriteID spriteId, Int x, Int y)
{
    const ScreenConfiguration* pConfig = pScreen->screenConfig;
    UInt16* pSprite = pScreen->sprite[spriteId];
    const UInt16 hshift = (pConfig->spr_shift & 0xf0) >> 4;
    const UInt16 vshift = pConfig->spr_shift & 0x0f;
    const UInt16 osprxpos = pSprite[0];
    const UInt16 osprxctl = pSprite[1];
    const UInt16 osvstart = (osprxpos >> 8) | (((osprxctl >> 2) & 0x01) << 8);
    const UInt16 osvstop = (osprxctl >> 8) | (((osprxctl >> 1) & 0x01) << 8);
    const UInt16 sprhigh = osvstop - osvstart;
    const UInt16 hstart = pConfig->diw_start_h - 1 + (x >> hshift);
    const UInt16 vstart = pConfig->diw_start_v + (y >> vshift);
    const UInt16 vstop = vstart + sprhigh;
    const UInt16 sprxpos = ((vstart & 0x00ff) << 8) | ((hstart & 0x01fe) >> 1);
    const UInt16 sprxctl = ((vstop & 0x00ff) << 8) | (((vstart >> 8) & 0x0001) << 2) | (((vstop >> 8) & 0x0001) << 1) | (hstart & 0x0001);

    pSprite[0] = sprxpos;
    pSprite[1] = sprxctl;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: GraphicsDriver
////////////////////////////////////////////////////////////////////////////////

static const ColorTable gDefaultColorTable = {
    0x0000,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0fff,
    0x0000,     // XXX Mouse cursor
    0x0000      // XXX Mouse cursor
};


// Creates a graphics driver instance with a framebuffer based on the given video
// configuration and pixel format.
ErrorCode GraphicsDriver_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, GraphicsDriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    GraphicsDriver* pDriver;
    Screen* pScreen;
    
    try(kalloc_cleared(sizeof(GraphicsDriver), (Byte**) &pDriver));
    pDriver->isLightPenEnabled = false;
    Lock_Init(&pDriver->lock);


    // Allocate the null sprite
    try(kalloc_options(sizeof(UInt16)*2, KALLOC_OPTION_UNIFIED, (Byte**) &pDriver->nullSprite));
    pDriver->nullSprite[0] = 0;
    pDriver->nullSprite[1] = 0;
    

    // Allocate the Copper tools
    CopperScheduler_Init(&pDriver->copperScheduler);


    // Allocate a new screen
//    pConfig = &kScreenConfig_NTSC_320_200_60;
//    pConfig = &kScreenConfig_PAL_640_512_25;
    try(Screen_Create(pConfig, pixelFormat, pDriver->nullSprite, &pScreen));


    // Initialize vblank tools
    Semaphore_Init(&pDriver->vblank_sema, 0);
    try(InterruptController_AddDirectInterruptHandler(
        gInterruptController,
        INTERRUPT_ID_VERTICAL_BLANK,
        INTERRUPT_HANDLER_PRIORITY_NORMAL,
        (InterruptHandler_Closure)GraphicsDriver_VerticalBlankInterruptHandler,
        (Byte*)pDriver, &pDriver->vb_irq_handler)
    );
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, 
        pDriver->vb_irq_handler,
        true);


    // Initialize the video config related stuff
    GraphicsDriver_SetCLUT(pDriver, &gDefaultColorTable);


    // Activate the screen
    try(GraphicsDriver_SetCurrentScreen_Locked(pDriver, pScreen));

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
        GraphicsDriver_StopVideoRefresh_Locked(pDriver);
        
        try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, pDriver->vb_irq_handler));
        pDriver->vb_irq_handler = 0;
        
        Semaphore_Deinit(&pDriver->vblank_sema);
        CopperScheduler_Deinit(&pDriver->copperScheduler);
        
        Screen_Destroy(pDriver->screen);
        pDriver->screen = NULL;
        
        kfree((Byte*)pDriver->nullSprite);
        pDriver->nullSprite = NULL;

        Lock_Deinit(&pDriver->lock);
        
        kfree((Byte*)pDriver);
    }
}

const ScreenConfiguration* _Nonnull GraphicsDriver_GetCurrentScreenConfiguration(GraphicsDriverRef _Nonnull pDriver)
{
    Lock_Lock(&pDriver->lock);
    const ScreenConfiguration* pConfig = pDriver->screen->screenConfig;
    Lock_Unlock(&pDriver->lock);
    return pConfig;
}

// Returns a reference to the currently active framebuffer. NULL is returned if
// no framebuffer is active which implies that the video signal generator is
// turned off.
// \return the framebuffer or NULL
#define GraphicsDriver_GetFramebuffer_Locked(pDriver) \
    pDriver->screen->framebuffer

Surface* _Nullable GraphicsDriver_GetFramebuffer(GraphicsDriverRef _Nonnull pDriver)
{
    Lock_Lock(&pDriver->lock);
    Surface* pFramebuffer = GraphicsDriver_GetFramebuffer_Locked(pDriver);
    Lock_Unlock(&pDriver->lock);

    return pFramebuffer;
}

void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull pDriver)
{
    CopperScheduler_Run(&pDriver->copperScheduler);
    Semaphore_ReleaseFromInterruptContext(&pDriver->vblank_sema);
}

Size GraphicsDriver_GetFramebufferSize(GraphicsDriverRef _Nonnull pDriver)
{
    Lock_Lock(&pDriver->lock);
    Surface* pFramebuffer = GraphicsDriver_GetFramebuffer_Locked(pDriver);
    Lock_Unlock(&pDriver->lock);

    return (pFramebuffer) ? Surface_GetPixelSize(pFramebuffer) : Size_Zero;
}

// Stops the video refresh circuitry
void GraphicsDriver_StopVideoRefresh_Locked(GraphicsDriverRef _Nonnull pDriver)
{
    CHIPSET_BASE_DECL(cp);

    *CHIPSET_REG_16(cp, DMACON) = (DMAF_COPPER | DMAF_RASTER | DMAF_SPRITE | DMAF_BLITTER);
}

// Waits for a vblank to occur. This function acts as a vblank barrier meaning
// that it will wait for some vblank to happen after this function has been invoked.
// No vblank that occured before this function was called will make it return.
static ErrorCode GraphicsDriver_WaitForVerticalBlank_Locked(GraphicsDriverRef _Nonnull pDriver)
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

// Compiles the Copper program(s) for the currently active screen and schedules
// their execution by the Copper. Note that this function typically returns
// before the Copper program has started running.
static ErrorCode GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(GraphicsDriverRef _Nonnull pDriver)
{
    decl_try_err();
    Screen* pScreen = pDriver->screen;
    CopperProgram* oddFieldProg;
    CopperProgram* evenFieldProg;

    if (pScreen->isInterlaced) {
        try(CopperProgram_CreateScreenRefresh(pScreen, pDriver->isLightPenEnabled, true, &oddFieldProg));
        try(CopperProgram_CreateScreenRefresh(pScreen, pDriver->isLightPenEnabled, false, &evenFieldProg));
    } else {
        try(CopperProgram_CreateScreenRefresh(pScreen, pDriver->isLightPenEnabled, true, &oddFieldProg));
        evenFieldProg = NULL;
    }

    CopperScheduler_ScheduleProgram(&pDriver->copperScheduler, oddFieldProg, evenFieldProg);
    return EOK;

catch:
    return err;
}

// Sets the given screen as the current screen on the graphics driver. All graphics
// command apply to this new screen once this function has returned.
// \param pNewScreen the new screen
// \return the error code
ErrorCode GraphicsDriver_SetCurrentScreen_Locked(GraphicsDriverRef _Nonnull pDriver, Screen* _Nonnull pNewScreen)
{
    decl_try_err();
    Screen* pOldScreen = pDriver->screen;
    Bool hasSwitchScreens = false;

    
    // Update the graphics device state.
    pDriver->screen = pNewScreen;
    

    // Turn video refresh back on and point it to the new copper program
    try(GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(pDriver));
    hasSwitchScreens = true;
    

    // Wait for the vblank. Once we got a vblank we know that the DMA is no longer
    // accessing the old framebuffer
    try(GraphicsDriver_WaitForVerticalBlank_Locked(pDriver));
    
    
    // Free the old screen
    Screen_Destroy(pOldScreen);

    return EOK;

catch:
    if (!hasSwitchScreens) {
        pDriver->screen = pOldScreen;
    }
    return err;
}

// Enables / disables the h/v raster position latching triggered by a light pen.
ErrorCode GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull pDriver, Bool enabled)
{
    decl_try_err();

    Lock_Lock(&pDriver->lock);
    if (pDriver->isLightPenEnabled != enabled) {
        pDriver->isLightPenEnabled = enabled;
        try(GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(pDriver));
    }
    Lock_Unlock(&pDriver->lock);

    return EOK;

catch:
    Lock_Unlock(&pDriver->lock);
    return err;
}

// Returns the current position of the light pen if the light pen triggered.
Bool GraphicsDriver_GetLightPenPosition(GraphicsDriverRef _Nonnull pDriver, Int16* _Nonnull pPosX, Int16* _Nonnull pPosY)
{
    CHIPSET_BASE_DECL(cp);
    Bool r = false;

    Lock_Lock(&pDriver->lock);
    
    // Read VHPOSR first time
    const Int32 posr0 = *CHIPSET_REG_16(cp, VPOSR);
    
    // XXX wait for scanline microseconds
    for (int i = 0; i < 1000; i++) {}
    
    // Read VHPOSR a second time
    const Int32 posr1 = *CHIPSET_REG_16(cp, VPOSR);
    
    
    // Check whether the light pen triggered
    // See Amiga Reference Hardware Manual p233.
    if (posr0 == posr1) {
        if ((posr0 & 0x0000ffff) < 0x10500) {
            *pPosX = (posr0 & 0x000000ff) << 1;
            *pPosY = (posr0 & 0x1ff00) >> 8;
            
            if (pDriver->screen->isInterlaced && posr0 < 0) {
                // long frame (odd field) is offset in Y by one
                *pPosY += 1;
            }
            r = true;
        }
    }
    Lock_Unlock(&pDriver->lock);

    return r;
}

// Acquires a hardware sprite
ErrorCode GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull pDriver, const UInt16* _Nonnull pPlanes[2], Int x, Int y, Int width, Int height, Int priority, SpriteID* _Nonnull pOutSpriteId)
{
    decl_try_err();

    Lock_Lock(&pDriver->lock);
    try(Screen_AcquireSprite(pDriver->screen, pPlanes, x, y, width, height, priority, pOutSpriteId));
    try(GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(pDriver));
    Lock_Unlock(&pDriver->lock);

    return EOK;

catch:
    Lock_Unlock(&pDriver->lock);
    *pOutSpriteId = -1;
    return err;
}

// Relinquishes a hardware sprite
ErrorCode GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull pDriver, SpriteID spriteId)
{
    decl_try_err();

    Lock_Lock(&pDriver->lock);
    Screen_RelinquishSprite(pDriver->screen, spriteId);
    try(GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(pDriver));
    Lock_Unlock(&pDriver->lock);

    return EOK;

catch:
    Lock_Unlock(&pDriver->lock);
    return err; // XXX clarify whether that's a thing or not
}

// Updates the position of a hardware sprite
ErrorCode GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull pDriver, SpriteID spriteId, Int x, Int y)
{
    decl_try_err();

    Lock_Lock(&pDriver->lock);
    Screen_SetSpritePosition(pDriver->screen, spriteId, x, y);
    try (GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(pDriver));
    Lock_Unlock(&pDriver->lock);

    return EOK;

catch:
    Lock_Unlock(&pDriver->lock);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Drawing
////////////////////////////////////////////////////////////////////////////////

// Writes the given RGB color to the color register at index idx
void GraphicsDriver_SetCLUTEntry(GraphicsDriverRef _Nonnull pDriver, Int idx, const RGBColor* _Nonnull pColor)
{
    assert(idx >= 0 && idx < CLUT_ENTRY_COUNT);
    const UInt16 rgb = (pColor->r >> 4 & 0x0f) << 8 | (pColor->g >> 4 & 0x0f) << 4 | (pColor->b >> 4 & 0x0f);
    CHIPSET_BASE_DECL(cp);

    *CHIPSET_REG_16(cp, COLOR_BASE + (idx << 1)) = rgb;
}

// Sets the CLUT
void GraphicsDriver_SetCLUT(GraphicsDriverRef _Nonnull pDriver, const ColorTable* pCLUT)
{
    CHIPSET_BASE_DECL(cp);

    for (Int i = 0; i < CLUT_ENTRY_COUNT; i++) {
        *CHIPSET_REG_16(cp, COLOR_BASE + (i << 1)) = pCLUT->entry[i];
    }
}

// Fills the framebuffer with the background color. This is black for RGB direct
// pixel formats and index 0 for RGB indexed pixel formats.
void GraphicsDriver_Clear(GraphicsDriverRef _Nonnull pDriver)
{
    Lock_Lock(&pDriver->lock);
    const Surface* pSurface = GraphicsDriver_GetFramebuffer_Locked(pDriver);
    assert(pSurface);

    const Int nbytes = pSurface->bytesPerRow * pSurface->height;
    for (Int i = 0; i < pSurface->planeCount; i++) {
        Bytes_ClearRange(pSurface->planes[i], nbytes);
    }
    Lock_Unlock(&pDriver->lock);
}

// Fills the pixels in the given rectangular framebuffer area with the given color.
void GraphicsDriver_FillRect(GraphicsDriverRef _Nonnull pDriver, Rect rect, Color color)
{
    Lock_Lock(&pDriver->lock);
    const Surface* pSurface = GraphicsDriver_GetFramebuffer_Locked(pDriver);
    assert(pSurface);

    const Rect bounds = Rect_Make(0, 0, pSurface->width, pSurface->height);
    const Rect r = Rect_Intersection(rect, bounds);
    
    if (Rect_IsEmpty(r)) {
        Lock_Unlock(&pDriver->lock);
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
    Lock_Unlock(&pDriver->lock);
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
    
    Lock_Lock(&pDriver->lock);
    const Surface* pSurface = GraphicsDriver_GetFramebuffer_Locked(pDriver);
    assert(pSurface);

    const Rect src_r = srcRect;
    const Rect dst_r = Rect_Make(dstLoc.x, dstLoc.y, src_r.width, src_r.height);
    const Int fb_width = pSurface->width;
    const Int fb_height = pSurface->height;
    const Int bytesPerRow = pSurface->bytesPerRow;
    const Int src_end_y = src_r.y + src_r.height - 1;
    const Int dst_clipped_left_span = (dst_r.x < 0) ? -dst_r.x : 0;
    const Int dst_clipped_right_span = __max(dst_r.x + dst_r.width - fb_width, 0);
    const Int dst_x = __max(dst_r.x, 0);
    const Int src_x = src_r.x + dst_clipped_left_span;
    const Int dst_width = __max(dst_r.width - dst_clipped_left_span - dst_clipped_right_span, 0);

    for (Int i = 0; i < pSurface->planeCount; i++) {
        Byte* pPlane = pSurface->planes[i];

        if (dst_r.y >= src_r.y && dst_r.y <= src_end_y) {
            const Int dst_clipped_y_span = __max(dst_r.y + dst_r.height - fb_height, 0);
            const Int dst_y_min = __max(dst_r.y, 0);
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
            Int dst_y = __max(dst_r.y, 0);
            const Int dst_y_max = __min(dst_r.y + dst_r.height, fb_height);
            Int src_y = src_r.y + dst_clipped_y_span;
            
            while (dst_y < dst_y_max) {
                Bits_CopyRange(BitPointer_Make(pPlane + dst_y * bytesPerRow, dst_x),
                               BitPointer_Make(pPlane + src_y * bytesPerRow, src_x),
                               dst_width);
                src_y++; dst_y++;
            }
        }
    }
    Lock_Unlock(&pDriver->lock);
}

// Blits a monochromatic 8x8 pixel glyph to the given position in the framebuffer.
void GraphicsDriver_BlitGlyph_8x8bw(GraphicsDriverRef _Nonnull pDriver, const Byte* _Nonnull pGlyphBitmap, Int x, Int y)
{
    Lock_Lock(&pDriver->lock);
    const Surface* pSurface = GraphicsDriver_GetFramebuffer_Locked(pDriver);
    assert(pSurface);

    const Int bytesPerRow = pSurface->bytesPerRow;
    const Int maxX = pSurface->width >> 3;
    const Int maxY = pSurface->height >> 3;
    
    if (x < 0 || y < 0 || x >= maxX || y >= maxY) {
        Lock_Unlock(&pDriver->lock);
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
    Lock_Unlock(&pDriver->lock);
}
