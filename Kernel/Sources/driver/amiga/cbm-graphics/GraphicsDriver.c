//
//  GraphicsDriver.c
//  kernel
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


int ScreenConfiguration_GetPixelWidth(const ScreenConfiguration* pConfig)
{
    return pConfig->width;
}

int ScreenConfiguration_GetPixelHeight(const ScreenConfiguration* pConfig)
{
    return pConfig->height;
}

int ScreenConfiguration_GetRefreshRate(const ScreenConfiguration* pConfig)
{
    return pConfig->fps;
}

bool ScreenConfiguration_IsInterlaced(const ScreenConfiguration* pConfig)
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
        kfree(pSprite->data);
        pSprite->data = NULL;
        
        kfree(pSprite);
    }
}

// Creates a sprite object.
static errno_t Sprite_Create(const uint16_t* _Nonnull pPlanes[2], int height, Sprite* _Nonnull * _Nonnull pOutSprite)
{
    decl_try_err();
    Sprite* pSprite;
    
    try(kalloc_cleared(sizeof(Sprite), (void**) &pSprite));
    pSprite->x = 0;
    pSprite->y = 0;
    pSprite->height = (uint16_t)height;
    pSprite->isVisible = true;


    // Construct the sprite DMA data
    const int nWords = 2 + 2*height + 2;
    try(kalloc_options(sizeof(uint16_t) * nWords, KALLOC_OPTION_UNIFIED, (void**) &pSprite->data));
    const uint16_t* sp0 = pPlanes[0];
    const uint16_t* sp1 = pPlanes[1];
    uint16_t* dp = pSprite->data;

    *dp++ = 0;  // sprxpos (will be filled out by the caller)
    *dp++ = 0;  // sprxctl (will be filled out by the caller)
    for (int i = 0; i < height; i++) {
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
    const uint16_t hshift = (pConfig->spr_shift & 0xf0) >> 4;
    const uint16_t vshift = pConfig->spr_shift & 0x0f;
    const uint16_t hstart = (pSprite->isVisible) ? pConfig->diw_start_h - 1 + (pSprite->x >> hshift) : 511;
    const uint16_t vstart = pConfig->diw_start_v + (pSprite->y >> vshift);
    const uint16_t vstop = vstart + pSprite->height;
    const uint16_t sprxpos = ((vstart & 0x00ff) << 8) | ((hstart & 0x01fe) >> 1);
    const uint16_t sprxctl = ((vstop & 0x00ff) << 8) | (((vstart >> 8) & 0x0001) << 2) | (((vstop >> 8) & 0x0001) << 1) | (hstart & 0x0001);

    pSprite->data[0] = sprxpos;
    pSprite->data[1] = sprxctl;
}

// Updates the position of a hardware sprite.
static inline void Sprite_SetPosition(Sprite* _Nonnull pSprite, int x, int y, const ScreenConfiguration* pConfig)
{
    pSprite->x = x;
    pSprite->y = y;
    Sprite_StateDidChange(pSprite, pConfig);
}

// Updates the visibility state of a hardware sprite.
static inline void Sprite_SetVisible(Sprite* _Nonnull pSprite, bool isVisible, const ScreenConfiguration* pConfig)
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
        
        kfree(pScreen);
    }
}

// Creates a screen object.
// \param pConfig the video configuration
// \param pixelFormat the pixel format (must be supported by the config)
// \return the screen or null
static errno_t Screen_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, Sprite* _Nonnull pNullSprite, Screen* _Nullable * _Nonnull pOutScreen)
{
    decl_try_err();
    Screen* pScreen;
    
    try(kalloc_cleared(sizeof(Screen), (void**) &pScreen));
    
    pScreen->screenConfig = pConfig;
    pScreen->pixelFormat = pixelFormat;
    pScreen->nullSprite = pNullSprite;
    pScreen->isInterlaced = ScreenConfiguration_IsInterlaced(pConfig);
    pScreen->clutCapacity = PixelFormat_GetCLUTCapacity(pixelFormat);

    
    // Allocate an appropriate framebuffer
    try(Surface_Create(pConfig->width, pConfig->height, pixelFormat, &pScreen->framebuffer));
    
    
    // Lock the new surface
    try(Surface_LockPixels(pScreen->framebuffer, kSurfaceAccess_Read|kSurfaceAccess_ReadWrite));
    
    *pOutScreen = pScreen;
    return EOK;
    
catch:
    Screen_Destroy(pScreen);
    *pOutScreen = NULL;
    return err;
}

static errno_t Screen_AcquireSprite(Screen* _Nonnull pScreen, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, SpriteID* _Nonnull pOutSpriteId)
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
static errno_t Screen_RelinquishSprite(Screen* _Nonnull pScreen, SpriteID spriteId)
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
static errno_t Screen_SetSpritePosition(Screen* _Nonnull pScreen, SpriteID spriteId, int x, int y)
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
static errno_t Screen_SetSpriteVisible(Screen* _Nonnull pScreen, SpriteID spriteId, bool isVisible)
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

static const RGBColor32 gDefaultColors[32] = {
    0xff000000,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xff000000,     // XXX Mouse cursor
    0xff000000,     // XXX Mouse cursor
};

static const ColorTable gDefaultColorTable = {
    32,
    gDefaultColors
};


// Creates a graphics driver instance with a framebuffer based on the given video
// configuration and pixel format.
errno_t GraphicsDriver_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, GraphicsDriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    GraphicsDriver* pDriver;
    Screen* pScreen;
    
    try(Object_Create(GraphicsDriver, &pDriver));
    pDriver->isLightPenEnabled = false;
    Lock_Init(&pDriver->lock);
    

    // Allocate the mouse painter
    try(MousePainter_Init(&pDriver->mousePainter));


    // Allocate the Copper tools
    CopperScheduler_Init(&pDriver->copperScheduler);


    // Allocate the null sprite
    const uint16_t* nullSpritePlanes[2];
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
        pDriver, &pDriver->vb_irq_handler)
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
    Object_Release(pDriver);
    *pOutDriver = NULL;
    return err;
}

// Deallocates the given graphics driver.
void GraphicsDriver_deinit(GraphicsDriverRef _Nonnull pDriver)
{
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
}

void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull pDriver)
{
    CopperScheduler_Run(&pDriver->copperScheduler);
    MousePainter_Paint_VerticalBlank(&pDriver->mousePainter);
    Semaphore_RelinquishFromInterruptContext(&pDriver->vblank_sema);
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

// Stops the video refresh circuitry
void GraphicsDriver_StopVideoRefresh_Locked(GraphicsDriverRef _Nonnull pDriver)
{
    CHIPSET_BASE_DECL(cp);

    *CHIPSET_REG_16(cp, DMACON) = (DMACONF_COPEN | DMACONF_BPLEN | DMACONF_SPREN | DMACONF_BLTEN);
}

// Waits for a vblank to occur. This function acts as a vblank barrier meaning
// that it will wait for some vblank to happen after this function has been invoked.
// No vblank that occured before this function was called will make it return.
static errno_t GraphicsDriver_WaitForVerticalBlank_Locked(GraphicsDriverRef _Nonnull pDriver)
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
static errno_t GraphicsDriver_CompileAndScheduleCopperProgramsAsync_Locked(GraphicsDriverRef _Nonnull pDriver)
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
errno_t GraphicsDriver_SetCurrentScreen_Locked(GraphicsDriverRef _Nonnull pDriver, Screen* _Nonnull pNewScreen)
{
    decl_try_err();
    Screen* pOldScreen = pDriver->screen;
    bool wasMouseCursorVisible = pDriver->mousePainter.flags.isVisible;
    bool hasSwitchedScreens = false;

    
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
    
    
    // Associate the mouse painter with the new screen
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
errno_t GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull pDriver, bool enabled)
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
bool GraphicsDriver_GetLightPenPosition(GraphicsDriverRef _Nonnull pDriver, int16_t* _Nonnull pPosX, int16_t* _Nonnull pPosY)
{
    CHIPSET_BASE_DECL(cp);
    
    // Read VHPOSR first time
    const uint32_t posr0 = *CHIPSET_REG_32(cp, VPOSR);


    // Wait for scanline microseconds
    const uint32_t hsync0 = chipset_get_hsync_counter();
    const uint16_t bplcon0 = *CHIPSET_REG_16(cp, BPLCON0);
    while (chipset_get_hsync_counter() == hsync0);
    

    // Read VHPOSR a second time
    const uint32_t posr1 = *CHIPSET_REG_32(cp, VPOSR);
    

    
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
errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull pDriver, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, SpriteID* _Nonnull pOutSpriteId)
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
errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull pDriver, SpriteID spriteId)
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
errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull pDriver, SpriteID spriteId, int x, int y)
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
errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull pDriver, SpriteID spriteId, bool isVisible)
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

void GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull pDriver, const void* pBitmap, const void* pMask)
{
    Lock_Lock(&pDriver->lock);
    MousePainter_SetCursor(&pDriver->mousePainter, pBitmap, pMask);
    Lock_Unlock(&pDriver->lock);
}

void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull pDriver, bool isVisible)
{
    Lock_Lock(&pDriver->lock);
    MousePainter_SetVisible(&pDriver->mousePainter, isVisible);
    Lock_Unlock(&pDriver->lock);
}

void GraphicsDriver_SetMouseCursorHiddenUntilMouseMoves(GraphicsDriverRef _Nonnull pDriver, bool flag)
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

void GraphicsDriver_SetMouseCursorPositionFromInterruptContext(GraphicsDriverRef _Nonnull pDriver, int16_t x, int16_t y)
{
    MousePainter_SetPosition_VerticalBlank(&pDriver->mousePainter, x, y);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Framebuffer
////////////////////////////////////////////////////////////////////////////////

Size GraphicsDriver_GetFramebufferSize(GraphicsDriverRef _Nonnull self)
{
    Size fbSize;

    Lock_Lock(&self->lock);
    Surface* pFramebuffer = self->screen->framebuffer;
    if (pFramebuffer) {
        fbSize = Size_Make(Surface_GetWidth(pFramebuffer), Surface_GetHeight(pFramebuffer));
    }
    else {
        fbSize = Size_Zero;
    }
    Lock_Unlock(&self->lock);

    return fbSize;
}

errno_t GraphicsDriver_LockFramebufferPixels(GraphicsDriverRef _Nonnull self, SurfaceAccess access, void* _Nonnull plane[8], size_t bytesPerRow[8], size_t* _Nonnull planeCount)
{
    decl_try_err();

    Lock_Lock(&self->lock);

    Surface* pSurface = self->screen->framebuffer;

    if (pSurface == NULL) {
        throw(ENODEV);
    }

    for (int8_t i = 0; i < pSurface->planeCount; i++) {
        plane[i] = pSurface->plane[i];
        bytesPerRow[i] = pSurface->bytesPerRow;
    }
    *planeCount = pSurface->planeCount;

    MousePainter_ShieldCursor(&self->mousePainter, Rect_Make(0, 0, pSurface->width, pSurface->height));

catch:
    Lock_Unlock(&self->lock);
    return EOK;
}

void GraphicsDriver_UnlockFramebufferPixels(GraphicsDriverRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    MousePainter_UnshieldCursor(&self->mousePainter);
    Lock_Unlock(&self->lock);
}

static uint16_t RGBColor12_Make(RGBColor32 clr)
{
    const uint8_t r = RGBColor32_GetRed(clr);
    const uint8_t g = RGBColor32_GetGreen(clr);
    const uint8_t b = RGBColor32_GetBlue(clr);

    return (uint16_t)(r >> 4 & 0x0f) << 8 | (g >> 4 & 0x0f) << 4 | (b >> 4 & 0x0f);
}

// Writes the given RGB color to the color register at index idx
errno_t GraphicsDriver_SetCLUTEntry(GraphicsDriverRef _Nonnull pDriver, int idx, RGBColor32 color)
{
    decl_try_err();

    Lock_Lock(&pDriver->lock);

    // Need to be able to access all CLUT entries in a screen even if the screen
    // supports < MAX_CLUT_ENTRIES (because of sprites).
    if (idx < 0 || idx >= MAX_CLUT_ENTRIES) {
        throw(EINVAL);
    }

    CHIPSET_BASE_DECL(cp);
    *CHIPSET_REG_16(cp, COLOR_BASE + (idx << 1)) = RGBColor12_Make(color);

catch:
    Lock_Unlock(&pDriver->lock);
    return err;
}

// Sets the CLUT
void GraphicsDriver_SetCLUT(GraphicsDriverRef _Nonnull pDriver, const ColorTable* pCLUT)
{
    Lock_Lock(&pDriver->lock);

    CHIPSET_BASE_DECL(cp);

    int count = __min(pCLUT->entryCount, pDriver->screen->clutCapacity);
    for (int i = 0; i < count; i++) {
        const RGBColor32 color = pCLUT->entry[i];
        const uint16_t rgb12 = RGBColor12_Make(color);

        *CHIPSET_REG_16(cp, COLOR_BASE + (i << 1)) = rgb12;
    }

    Lock_Unlock(&pDriver->lock);
}


CLASS_METHODS(GraphicsDriver, Object,
OVERRIDE_METHOD_IMPL(deinit, GraphicsDriver, Object)
);
