//
//  GraphicsDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"

const char* const kFramebufferName = "fb";


// Creates a graphics driver instance with a framebuffer based on the given video
// configuration and pixel format.
errno_t GraphicsDriver_Create(DriverRef _Nullable parent, const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, GraphicsDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    GraphicsDriverRef self;
    Screen* pScreen;
    
    try(Driver_Create(GraphicsDriver, 0, parent, &self));
    self->isLightPenEnabled = false;
    Lock_Init(&self->lock);
    

    // Allocate the mouse painter
    try(MousePainter_Init(&self->mousePainter));


    // Allocate the Copper tools
    CopperScheduler_Init(&self->copperScheduler);


    // Allocate the null sprite
    const uint16_t* nullSpritePlanes[2];
    nullSpritePlanes[0] = NULL;
    nullSpritePlanes[1] = NULL;
    try(Sprite_Create(nullSpritePlanes, 0, &kScreenConfig_NTSC_320_200_60, &self->nullSprite));


    // Allocate a new screen
//    pConfig = &kScreenConfig_NTSC_320_200_60;
//    pConfig = &kScreenConfig_PAL_640_512_25;
    try(Screen_Create(pConfig, pixelFormat, self->nullSprite, &pScreen));


    // Initialize vblank tools
    Semaphore_Init(&self->vblank_sema, 0);
    try(InterruptController_AddDirectInterruptHandler(
        gInterruptController,
        INTERRUPT_ID_VERTICAL_BLANK,
        INTERRUPT_HANDLER_PRIORITY_NORMAL,
        (InterruptHandler_Closure)GraphicsDriver_VerticalBlankInterruptHandler,
        self, &self->vb_irq_handler)
    );
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, 
        self->vb_irq_handler,
        true);


    // Activate the screen
    try(GraphicsDriver_SetCurrentScreen_Locked(self, pScreen));

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

// Deallocates the given graphics driver.
void GraphicsDriver_deinit(GraphicsDriverRef _Nonnull self)
{
    GraphicsDriver_StopVideoRefresh_Locked(self);
        
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->vb_irq_handler));
    self->vb_irq_handler = 0;
        
    Screen_Destroy(self->screen);
    self->screen = NULL;

    Sprite_Destroy(self->nullSprite);
    self->nullSprite = NULL;

    Semaphore_Deinit(&self->vblank_sema);
    CopperScheduler_Deinit(&self->copperScheduler);

    MousePainter_Deinit(&self->mousePainter);

    Lock_Deinit(&self->lock);
}

void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull self)
{
    CopperScheduler_Run(&self->copperScheduler);
    MousePainter_Paint_VerticalBlank(&self->mousePainter);
    Semaphore_RelinquishFromInterruptContext(&self->vblank_sema);
}

static errno_t GraphicsDriver_onStart(DriverRef _Nonnull _Locked self)
{
    return Driver_Publish(self, kFramebufferName, 0);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Properties
////////////////////////////////////////////////////////////////////////////////

const ScreenConfiguration* _Nonnull GraphicsDriver_GetCurrentScreenConfiguration(GraphicsDriverRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const ScreenConfiguration* pConfig = self->screen->screenConfig;
    Lock_Unlock(&self->lock);
    return pConfig;
}

// Stops the video refresh circuitry
void GraphicsDriver_StopVideoRefresh_Locked(GraphicsDriverRef _Nonnull self)
{
    CHIPSET_BASE_DECL(cp);

    *CHIPSET_REG_16(cp, DMACON) = (DMACONF_COPEN | DMACONF_BPLEN | DMACONF_SPREN | DMACONF_BLTEN);
}

// Waits for a vblank to occur. This function acts as a vblank barrier meaning
// that it will wait for some vblank to happen after this function has been invoked.
// No vblank that occurred before this function was called will make it return.
static errno_t GraphicsDriver_WaitForVerticalBlank_Locked(GraphicsDriverRef _Nonnull self)
{
    decl_try_err();

    // First purge the vblank sema to ensure that we don't accidentally pick up
    // some vblank that has happened before this function has been called. Then
    // wait for the actual vblank.
    try(Semaphore_TryAcquire(&self->vblank_sema));
    try(Semaphore_Acquire(&self->vblank_sema, kTimeInterval_Infinity));
    return EOK;

catch:
    return err;
}

// Compiles a Copper program to display a non-interlaced screen or a single
// field of an interlaced screen.
static errno_t create_copper_prog(Screen* _Nonnull pScreen, size_t instrCount, bool isLightPenEnabled, bool isOddField, CopperProgram* _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    CopperProgram* prog;
    
    err = CopperProgram_Create(instrCount, &prog);
    if (err == EOK) {
        CopperInstruction* ip = prog->entry;

        ip = Screen_MakeCopperProgram(pScreen, ip, isLightPenEnabled, isOddField);

        // end instruction
        *ip = COP_END();
    }
    
    *pOutProg = prog;
    return err;
}

// Triggers an update of the display so that it accurately reflects the current
// display configuration.
static errno_t GraphicsDriver_UpdateDisplay_Locked(GraphicsDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    Screen* pScreen = self->screen;

    if (pScreen->flags.isNewCopperProgNeeded) {
        const size_t instrCount = Screen_CalcCopperProgramLength(pScreen) + 1;
        CopperProgram* oddFieldProg = NULL;
        CopperProgram* evenFieldProg = NULL;

        try(create_copper_prog(pScreen, instrCount, self->isLightPenEnabled, true, &oddFieldProg));

        if (Screen_IsInterlaced(pScreen)) {
            try(create_copper_prog(pScreen, instrCount, self->isLightPenEnabled, false, &evenFieldProg));
        }

        CopperScheduler_ScheduleProgram(&self->copperScheduler, oddFieldProg, evenFieldProg);
        pScreen->flags.isNewCopperProgNeeded = false;
    }

catch:
    return err;
}

// Triggers an update of the display so that it accurately reflects the current
// display configuration.
errno_t GraphicsDriver_UpdateDisplay(GraphicsDriverRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const errno_t err = GraphicsDriver_UpdateDisplay_Locked(self);
    Lock_Unlock(&self->lock);
    return err;
}

// Sets the given screen as the current screen on the graphics driver. All graphics
// command apply to this new screen once this function has returned.
// \param pNewScreen the new screen
// \return the error code
errno_t GraphicsDriver_SetCurrentScreen_Locked(GraphicsDriverRef _Nonnull self, Screen* _Nonnull pNewScreen)
{
    decl_try_err();
    Screen* pOldScreen = self->screen;
    bool wasMouseCursorVisible = self->mousePainter.flags.isVisible;
    bool hasSwitchedScreens = false;

    
    // Disassociate the mouse painter from the old screen (hides the mouse cursor)
    MousePainter_SetSurface(&self->mousePainter, NULL);


    // Update the graphics device state.
    self->screen = pNewScreen;
    

    // Turn video refresh back on and point it to the new copper program
    try(GraphicsDriver_UpdateDisplay_Locked(self));
    hasSwitchedScreens = true;
    

    // Wait for the vblank. Once we got a vblank we know that the DMA is no longer
    // accessing the old framebuffer
    try(GraphicsDriver_WaitForVerticalBlank_Locked(self));
    
    
    // Associate the mouse painter with the new screen
    MousePainter_SetSurface(&self->mousePainter, pNewScreen->surface);
    MousePainter_SetVisible(&self->mousePainter, wasMouseCursorVisible);


    // Free the old screen
    Screen_Destroy(pOldScreen);

    return EOK;

catch:
    if (!hasSwitchedScreens) {
        self->screen = pOldScreen;
    }
    MousePainter_SetSurface(&self->mousePainter, pOldScreen->surface);
    MousePainter_SetVisible(&self->mousePainter, wasMouseCursorVisible);
    return err;
}

// Enables / disables the h/v raster position latching triggered by a light pen.
errno_t GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull self, bool enabled)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    if (self->isLightPenEnabled != enabled) {
        self->isLightPenEnabled = enabled;
        Screen_SetNeedsUpdate(self->screen);
    }
    Lock_Unlock(&self->lock);

    return EOK;

catch:
    Lock_Unlock(&self->lock);
    return err;
}

// Returns the current position of the light pen if the light pen triggered.
bool GraphicsDriver_GetLightPenPosition(GraphicsDriverRef _Nonnull self, int16_t* _Nonnull pPosX, int16_t* _Nonnull pPosY)
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
errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull self, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, SpriteID* _Nonnull pOutSpriteId)
{
    Lock_Lock(&self->lock);
    const errno_t err = Screen_AcquireSprite(self->screen, pPlanes, x, y, width, height, priority, pOutSpriteId);
    Lock_Unlock(&self->lock);
    return err;
}

// Relinquishes a hardware sprite
errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull self, SpriteID spriteId)
{
    Lock_Lock(&self->lock);
    const errno_t err = Screen_RelinquishSprite(self->screen, spriteId);
    Lock_Unlock(&self->lock);
    return err;
}

// Updates the position of a hardware sprite.
errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, SpriteID spriteId, int x, int y)
{
    Lock_Lock(&self->lock);
    const errno_t err = Screen_SetSpritePosition(self->screen, spriteId, x, y);
    Lock_Unlock(&self->lock);
    return err;
}

// Updates the visibility of a hardware sprite.
errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, SpriteID spriteId, bool isVisible)
{
    Lock_Lock(&self->lock);
    const errno_t err = Screen_SetSpriteVisible(self->screen, spriteId, isVisible);
    Lock_Unlock(&self->lock);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

void GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull self, const void* pBitmap, const void* pMask)
{
    Lock_Lock(&self->lock);
    MousePainter_SetCursor(&self->mousePainter, pBitmap, pMask);
    Lock_Unlock(&self->lock);
}

void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull self, bool isVisible)
{
    Lock_Lock(&self->lock);
    MousePainter_SetVisible(&self->mousePainter, isVisible);
    Lock_Unlock(&self->lock);
}

void GraphicsDriver_SetMouseCursorHiddenUntilMove(GraphicsDriverRef _Nonnull self, bool flag)
{
    Lock_Lock(&self->lock);
    MousePainter_SetHiddenUntilMove(&self->mousePainter, flag);
    Lock_Unlock(&self->lock);
}

void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, Point loc)
{
    Lock_Lock(&self->lock);
    MousePainter_SetPosition(&self->mousePainter, loc);
    Lock_Unlock(&self->lock);
}

void GraphicsDriver_SetMouseCursorPositionFromInterruptContext(GraphicsDriverRef _Nonnull self, int16_t x, int16_t y)
{
    MousePainter_SetPosition_VerticalBlank(&self->mousePainter, x, y);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Framebuffer
////////////////////////////////////////////////////////////////////////////////

Size GraphicsDriver_GetFramebufferSize(GraphicsDriverRef _Nonnull self)
{
    Size fbSize;

    Lock_Lock(&self->lock);
    Screen_GetPixelSize(self->screen, &fbSize.width, &fbSize.height);
    Lock_Unlock(&self->lock);

    return fbSize;
}

errno_t GraphicsDriver_LockFramebufferPixels(GraphicsDriverRef _Nonnull self, PixelAccess access, void* _Nonnull plane[8], size_t bytesPerRow[8], size_t* _Nonnull planeCount)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = Screen_LockPixels(self->screen, access, plane, bytesPerRow, planeCount);
    if (err == EOK) {
        MousePainter_ShieldCursor(&self->mousePainter, Rect_Make(0, 0, self->screen->surface->width, self->screen->surface->height));
    }
    Lock_Unlock(&self->lock);
    return err;
}

errno_t GraphicsDriver_UnlockFramebufferPixels(GraphicsDriverRef _Nonnull self)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    MousePainter_UnshieldCursor(&self->mousePainter);
    err = Screen_UnlockPixels(self->screen);
    Lock_Unlock(&self->lock);
    return err;
}

// Writes the given RGB color to the color register at index idx
errno_t GraphicsDriver_SetCLUTEntry(GraphicsDriverRef _Nonnull self, size_t idx, RGBColor32 color)
{
    Lock_Lock(&self->lock);
    const errno_t err = Screen_SetCLUTEntry(self->screen, idx, color);
    Lock_Unlock(&self->lock);
    return err;
}

// Sets the contents of 'count' consecutive CLUT entries starting at index 'idx'
// to the colors in the array 'entries'.
errno_t GraphicsDriver_SetCLUTEntries(GraphicsDriverRef _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
{
    Lock_Lock(&self->lock);
    const errno_t err = Screen_SetCLUTEntries(self->screen, idx, count, entries);
    Lock_Unlock(&self->lock);
    return err;
}


class_func_defs(GraphicsDriver, Driver,
override_func_def(deinit, GraphicsDriver, Object)
override_func_def(onStart, GraphicsDriver, Driver)
);
