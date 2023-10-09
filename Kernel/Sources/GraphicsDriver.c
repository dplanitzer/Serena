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
    return (pConfig->bplcon0 & BPLCON0F_LACE) != 0;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprite
////////////////////////////////////////////////////////////////////////////////

static void Sprite_Destroy(Sprite* _Nullable pSprite)
{
    if (pSprite) {
        kfree((Byte*) pSprite->data);
        pSprite->data = NULL;
        
        kfree((Byte*)pSprite);
    }
}

// Creates a sprite object.
static ErrorCode Sprite_Create(const UInt16* _Nonnull pPlanes[2], Int height, Sprite* _Nonnull * _Nonnull pOutSprite)
{
    decl_try_err();
    Sprite* pSprite;
    
    try(kalloc_cleared(sizeof(Sprite), (Byte**) &pSprite));
    pSprite->x = 0;
    pSprite->y = 0;
    pSprite->height = (UInt16)height;
    pSprite->isVisible = true;


    // Construct the sprite DMA data
    const Int nWords = 2 + 2*height + 2;
    try(kalloc_options(sizeof(UInt16) * nWords, KALLOC_OPTION_UNIFIED, (Byte**) &pSprite->data));
    const UInt16* sp0 = pPlanes[0];
    const UInt16* sp1 = pPlanes[1];
    UInt16* dp = pSprite->data;

    *dp++ = 0;  // sprxpos (will be filled out by the caller)
    *dp++ = 0;  // sprxctl (will be filled out by the caller)
    for (Int i = 0; i < height; i++) {
        *dp++ = *sp0++;
        *dp++ = *sp1++;
    }
    *dp++ = 0;
    *dp   = 0;
    
    *pOutSprite = pSprite;
    return EOK;
    
catch:
    Sprite_Destroy(pSprite);
    *pOutSprite = NULL;
    return err;
}

// Called when the position or visibility of a hardware sprite has changed.
// Recalculates the sprxpos and sprxctl control words and updates them in the
// sprite DMA data block.
static void Sprite_StateDidChange(Sprite* _Nonnull pSprite, const ScreenConfiguration* pConfig)
{
    // Hiding a sprite means to move it all the way to X max.
    const UInt16 hshift = (pConfig->spr_shift & 0xf0) >> 4;
    const UInt16 vshift = pConfig->spr_shift & 0x0f;
    const UInt16 hstart = (pSprite->isVisible) ? pConfig->diw_start_h - 1 + (pSprite->x >> hshift) : 511;
    const UInt16 vstart = pConfig->diw_start_v + (pSprite->y >> vshift);
    const UInt16 vstop = vstart + pSprite->height;
    const UInt16 sprxpos = ((vstart & 0x00ff) << 8) | ((hstart & 0x01fe) >> 1);
    const UInt16 sprxctl = ((vstop & 0x00ff) << 8) | (((vstart >> 8) & 0x0001) << 2) | (((vstop >> 8) & 0x0001) << 1) | (hstart & 0x0001);

    pSprite->data[0] = sprxpos;
    pSprite->data[1] = sprxctl;
}

// Updates the position of a hardware sprite.
static inline void Sprite_SetPosition(Sprite* _Nonnull pSprite, Int x, Int y, const ScreenConfiguration* pConfig)
{
    pSprite->x = x;
    pSprite->y = y;
    Sprite_StateDidChange(pSprite, pConfig);
}

// Updates the visibility state of a hardware sprite.
static inline void Sprite_SetVisible(Sprite* _Nonnull pSprite, Bool isVisible, const ScreenConfiguration* pConfig)
{
    pSprite->isVisible = isVisible;
    Sprite_StateDidChange(pSprite, pConfig);
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
static ErrorCode Screen_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, Sprite* _Nonnull pNullSprite, Screen* _Nullable * _Nonnull pOutScreen)
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
    Sprite* pSprite;

    if (width < 0 || width > MAX_SPRITE_WIDTH) {
        throw(E2BIG);
    }
    if (height < 0 || height > MAX_SPRITE_HEIGHT) {
        throw(E2BIG);
    }
    if (priority < 0 || priority >= NUM_HARDWARE_SPRITES) {
        throw(EINVAL);
    }
    if (pScreen->sprite[priority]) {
        throw(EBUSY);
    }

    try(Sprite_Create(pPlanes, height, &pSprite));
    Sprite_SetPosition(pSprite, x, y, pConfig);

    pScreen->sprite[priority] = pSprite;
    pScreen->spritesInUseCount++;
    *pOutSpriteId = priority;

    return EOK;

catch:
    *pOutSpriteId = -1;
    return err;
}

// Relinquishes a hardware sprite
static ErrorCode Screen_RelinquishSprite(Screen* _Nonnull pScreen, SpriteID spriteId)
{
    decl_try_err();

    if (spriteId >= 0) {
        if (spriteId >= NUM_HARDWARE_SPRITES) {
            throw(EINVAL);
        }

        // XXX Sprite_Destroy(pScreen->sprite[spriteId]);
        // XXX actually free the old sprite instead of leaking it. Can't do this
        // XXX yet because we need to ensure that the DMA is no longer accessing
        // XXX the data before it freeing it.
        pScreen->sprite[spriteId] = pScreen->nullSprite;
        pScreen->spritesInUseCount--;
    }
    return EOK;

catch:
    return err;
}

// Updates the position of a hardware sprite.
static ErrorCode Screen_SetSpritePosition(Screen* _Nonnull pScreen, SpriteID spriteId, Int x, Int y)
{
    decl_try_err();

    if (spriteId < 0 || spriteId >= NUM_HARDWARE_SPRITES) {
        throw(EINVAL);
    }

    Sprite_SetPosition(pScreen->sprite[spriteId], x, y, pScreen->screenConfig);
    return EOK;

catch:
    return err;
}

// Updates the visibility of a hardware sprite.
static ErrorCode Screen_SetSpriteVisible(Screen* _Nonnull pScreen, SpriteID spriteId, Bool isVisible)
{
    decl_try_err();

    if (spriteId < 0 || spriteId >= NUM_HARDWARE_SPRITES) {
        throw(EINVAL);
    }

    Sprite_SetVisible(pScreen->sprite[spriteId], isVisible, pScreen->screenConfig);
    return EOK;

catch:
    return err;
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
    

    // Allocate the mouse painter
    try(MousePainter_Init(&pDriver->mousePainter));


    // Allocate the Copper tools
    CopperScheduler_Init(&pDriver->copperScheduler);


    // Allocate the null sprite
    const UInt16* nullSpritePlanes[2];
    nullSpritePlanes[0] = NULL;
    nullSpritePlanes[1] = NULL;
    try(Sprite_Create(nullSpritePlanes, 0, &pDriver->nullSprite));


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
        
        Screen_Destroy(pDriver->screen);
        pDriver->screen = NULL;

        Sprite_Destroy(pDriver->nullSprite);
        pDriver->nullSprite = NULL;

        Semaphore_Deinit(&pDriver->vblank_sema);
        CopperScheduler_Deinit(&pDriver->copperScheduler);

        MousePainter_Deinit(&pDriver->mousePainter);

        Lock_Deinit(&pDriver->lock);
        
        kfree((Byte*)pDriver);
    }
}

void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull pDriver)
{
    CopperScheduler_Run(&pDriver->copperScheduler);
    MousePainter_Paint_VerticalBlank(&pDriver->mousePainter);
    Semaphore_ReleaseFromInterruptContext(&pDriver->vblank_sema);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Properties
////////////////////////////////////////////////////////////////////////////////

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
    Bool wasMouseCursorVisible = pDriver->mousePainter.flags.isVisible;
    Bool hasSwitchedScreens = false;

    
    // Disassociate the mouse painter from the old screen (hides the mouse cursor)
    MousePainter_SetSurface(&pDriver->mousePainter, NULL);


    // Update the graphics device state.
    pDriver->screen = pNewScreen;
    

    // Turn video refresh back on and point it to the new copper program
    try(GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(pDriver));
    hasSwitchedScreens = true;
    

    // Wait for the vblank. Once we got a vblank we know that the DMA is no longer
    // accessing the old framebuffer
    try(GraphicsDriver_WaitForVerticalBlank_Locked(pDriver));
    
    
    // Associate the mouse painter with teh new screen
    MousePainter_SetSurface(&pDriver->mousePainter, pNewScreen->framebuffer);
    MousePainter_SetVisible(&pDriver->mousePainter, wasMouseCursorVisible);


    // Free the old screen
    Screen_Destroy(pOldScreen);

    return EOK;

catch:
    if (!hasSwitchedScreens) {
        pDriver->screen = pOldScreen;
    }
    MousePainter_SetSurface(&pDriver->mousePainter, pOldScreen->framebuffer);
    MousePainter_SetVisible(&pDriver->mousePainter, wasMouseCursorVisible);
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
    
    // Read VHPOSR first time
    const UInt32 posr0 = *CHIPSET_REG_32(cp, VPOSR);


    // Wait for scanline microseconds
    const UInt32 hsync0 = chipset_get_hsync_counter();
    const UInt16 bplcon0 = *CHIPSET_REG_16(cp, BPLCON0);
    while (chipset_get_hsync_counter() == hsync0);
    

    // Read VHPOSR a second time
    const UInt32 posr1 = *CHIPSET_REG_32(cp, VPOSR);
    
    
    
    // Check whether the light pen triggered
    // See Amiga Reference Hardware Manual p233.
    if (posr0 == posr1) {
        if ((posr0 & 0x0000ffff) < 0x10500) {
            *pPosX = (posr0 & 0x000000ff) << 1;
            *pPosY = (posr0 & 0x1ff00) >> 8;
            
            if ((bplcon0 & BPLCON0F_LACE) != 0 && ((posr0 & 0x8000) != 0)) {
                // long frame (odd field) is offset in Y by one
                *pPosY += 1;
            }
            return true;
        }
    }

    return false;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprites
////////////////////////////////////////////////////////////////////////////////

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
    try(Screen_RelinquishSprite(pDriver->screen, spriteId));
    try(GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(pDriver));
    Lock_Unlock(&pDriver->lock);

    return EOK;

catch:
    Lock_Unlock(&pDriver->lock);
    return err; // XXX clarify whether that's a thing or not
}

// Updates the position of a hardware sprite.
ErrorCode GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull pDriver, SpriteID spriteId, Int x, Int y)
{
    decl_try_err();

    Lock_Lock(&pDriver->lock);
    try(Screen_SetSpritePosition(pDriver->screen, spriteId, x, y));
    try (GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(pDriver));
    Lock_Unlock(&pDriver->lock);

    return EOK;

catch:
    Lock_Unlock(&pDriver->lock);
    return err;
}

// Updates the visibility of a hardware sprite.
ErrorCode GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull pDriver, SpriteID spriteId, Bool isVisible)
{
    decl_try_err();

    Lock_Lock(&pDriver->lock);
    try(Screen_SetSpriteVisible(pDriver->screen, spriteId, isVisible));
    try (GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(pDriver));
    Lock_Unlock(&pDriver->lock);

    return EOK;

catch:
    Lock_Unlock(&pDriver->lock);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

void GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull pDriver, const Byte* pBitmap, const Byte* pMask)
{
    Lock_Lock(&pDriver->lock);
    MousePainter_SetCursor(&pDriver->mousePainter, pBitmap, pMask);
    Lock_Unlock(&pDriver->lock);
}

void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull pDriver, Bool isVisible)
{
    Lock_Lock(&pDriver->lock);
    MousePainter_SetVisible(&pDriver->mousePainter, isVisible);
    Lock_Unlock(&pDriver->lock);
}

void GraphicsDriver_SetMouseCursorHiddenUntilMouseMoves(GraphicsDriverRef _Nonnull pDriver, Bool flag)
{
    Lock_Lock(&pDriver->lock);
    MousePainter_SetHiddenUntilMouseMoves(&pDriver->mousePainter, flag);
    Lock_Unlock(&pDriver->lock);
}

void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull pDriver, Point loc)
{
    Lock_Lock(&pDriver->lock);
    MousePainter_SetPosition(&pDriver->mousePainter, loc);
    Lock_Unlock(&pDriver->lock);
}

void GraphicsDriver_SetMouseCursorPositionFromInterruptContext(GraphicsDriverRef _Nonnull pDriver, Int16 x, Int16 y)
{
    MousePainter_SetPosition_VerticalBlank(&pDriver->mousePainter, x, y);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Drawing
////////////////////////////////////////////////////////////////////////////////

// Locks the graphics driver, retrieves a framebuffer reference and shields the
// mouse cursor. 'drawingArea' is the bounding box of the area into which the
// caller wants to draw.
static Surface* _Nonnull GraphicsDriver_BeginDrawing(GraphicsDriverRef _Nonnull pDriver, const Rect drawingArea)
{
    Lock_Lock(&pDriver->lock);

    Surface* pSurface = GraphicsDriver_GetFramebuffer_Locked(pDriver);
    assert(pSurface);

    MousePainter_ShieldCursor(&pDriver->mousePainter, drawingArea);
    return pSurface;
}

// Unlocks the graphics driver and restores the mouse cursor
static void GraphicsDriver_EndDrawing(GraphicsDriverRef _Nonnull pDriver)
{
    MousePainter_UnshieldCursor(&pDriver->mousePainter);
    Lock_Unlock(&pDriver->lock);
}

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
    Surface* pSurface = GraphicsDriver_BeginDrawing(pDriver, Rect_Infinite);
    const Int nbytes = pSurface->bytesPerRow * pSurface->height;
    for (Int i = 0; i < pSurface->planeCount; i++) {
        Bytes_ClearRange(pSurface->planes[i], nbytes);
    }
    GraphicsDriver_EndDrawing(pDriver);
}

// Fills the pixels in the given rectangular framebuffer area with the given color.
void GraphicsDriver_FillRect(GraphicsDriverRef _Nonnull pDriver, Rect rect, Color color)
{
    Surface* pSurface = GraphicsDriver_BeginDrawing(pDriver, rect);
    const Rect bounds = Rect_Make(0, 0, pSurface->width, pSurface->height);
    const Rect r = Rect_Intersection(rect, bounds);
    
    if (!Rect_IsEmpty(r)) {
        assert(color.tag == kColorType_Index);
    
        for (Int i = 0; i < pSurface->planeCount; i++) {
            const Bool bit = (color.u.index & (1 << i)) ? true : false;
        
            for (Int y = r.top; y < r.bottom; y++) {
                const BitPointer pBits = BitPointer_Make(pSurface->planes[i] + y * pSurface->bytesPerRow, r.left);
            
                if (bit) {
                    Bits_SetRange(pBits, Rect_GetWidth(r));
                } else {
                    Bits_ClearRange(pBits, Rect_GetWidth(r));
                }
            }
        }
    }
    GraphicsDriver_EndDrawing(pDriver);
}

// Copies the given rectangular framebuffer area to a different location in the framebuffer.
// Parts of the source rectangle which are outside the bounds of the framebuffer are treated as
// transparent. This means that the corresponding destination pixels will be left alone and not
// overwritten.
void GraphicsDriver_CopyRect(GraphicsDriverRef _Nonnull pDriver, Rect srcRect, Point dstLoc)
{
    if (Rect_IsEmpty(srcRect) || (srcRect.left == dstLoc.x && srcRect.top == dstLoc.y)) {
        return;
    }
    
    Surface* pSurface = GraphicsDriver_BeginDrawing(pDriver, Rect_Infinite);  // XXX calc tighter rect
    const Rect src_r = srcRect;
    const Rect dst_r = Rect_Make(dstLoc.x, dstLoc.y, dstLoc.x + Rect_GetWidth(src_r), dstLoc.y + Rect_GetHeight(src_r));
    const Int fb_width = pSurface->width;
    const Int fb_height = pSurface->height;
    const Int bytesPerRow = pSurface->bytesPerRow;
    const Int src_end_y = src_r.bottom - 1;
    const Int dst_clipped_left_span = (dst_r.left < 0) ? -dst_r.left : 0;
    const Int dst_clipped_right_span = __max(dst_r.right - fb_width, 0);
    const Int dst_x = __max(dst_r.left, 0);
    const Int src_x = src_r.left + dst_clipped_left_span;
    const Int dst_width = __max(Rect_GetWidth(dst_r) - dst_clipped_left_span - dst_clipped_right_span, 0);

    for (Int i = 0; i < pSurface->planeCount; i++) {
        Byte* pPlane = pSurface->planes[i];

        if (dst_r.top >= src_r.top && dst_r.top <= src_end_y) {
            const Int dst_clipped_y_span = __max(dst_r.bottom - fb_height, 0);
            const Int dst_y_min = __max(dst_r.top, 0);
            Int src_y = src_r.bottom - 1;
            Int dst_y = dst_r.bottom - dst_clipped_y_span - 1;
            
            while (dst_y >= dst_y_min) {
                Bits_CopyRange(BitPointer_Make(pPlane + dst_y * bytesPerRow, dst_x),
                               BitPointer_Make(pPlane + src_y * bytesPerRow, src_x),
                               dst_width);
                src_y--; dst_y--;
            }
        }
        else {
            const Int dst_clipped_y_span = (dst_r.top < 0) ? -dst_r.top : 0;
            Int dst_y = __max(dst_r.top, 0);
            const Int dst_y_max = __min(dst_r.bottom, fb_height);
            Int src_y = src_r.top + dst_clipped_y_span;
            
            while (dst_y < dst_y_max) {
                Bits_CopyRange(BitPointer_Make(pPlane + dst_y * bytesPerRow, dst_x),
                               BitPointer_Make(pPlane + src_y * bytesPerRow, src_x),
                               dst_width);
                src_y++; dst_y++;
            }
        }
    }
    GraphicsDriver_EndDrawing(pDriver);
}

// Blits a monochromatic 8x8 pixel glyph to the given position in the framebuffer.
void GraphicsDriver_BlitGlyph_8x8bw(GraphicsDriverRef _Nonnull pDriver, const Byte* _Nonnull pGlyphBitmap, Int x, Int y)
{
    Surface* pSurface = GraphicsDriver_BeginDrawing(pDriver, Rect_Make(x, y, x + 8, y + 8));
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

    GraphicsDriver_EndDrawing(pDriver);
}
