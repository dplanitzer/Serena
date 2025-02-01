//
//  GraphicsDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"

const char* const kFramebufferName = "fb";


// Creates a graphics driver instance which manages the on-board video hardware.
// We assume that video is turned off at the time this function is called and
// video remains turned off until a screen has been created and is made the
// current screen.
errno_t GraphicsDriver_Create(DriverRef _Nullable parent, GraphicsDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    GraphicsDriverRef self;
    
    try(Driver_Create(GraphicsDriver, 0, parent, &self));
    

    // Allocate the mouse painter
    try(MousePainter_Init(&self->mousePainter));


    // Allocate the Copper tools
    CopperScheduler_Init(&self->copperScheduler);


    // Allocate the null sprite
    const uint16_t* nullSpritePlanes[2];
    nullSpritePlanes[0] = NULL;
    nullSpritePlanes[1] = NULL;
    try(Sprite_Create(nullSpritePlanes, 0, &kScreenConfig_NTSC_320_200_60, &self->nullSprite));


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

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
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
// MARK: Display
////////////////////////////////////////////////////////////////////////////////

// Waits for a vblank to occur. This function acts as a vblank barrier meaning
// that it will wait for some vblank to happen after this function has been invoked.
// No vblank that occurred before this function was called will make it return.
static void GraphicsDriver_WaitForVerticalBlank_Locked(GraphicsDriverRef _Nonnull _Locked self)
{
    // First purge the vblank sema to ensure that we don't accidentally pick up
    // some vblank that has happened before this function has been called. Then
    // wait for the actual vblank.
    Semaphore_TryAcquire(&self->vblank_sema);
    Semaphore_Acquire(&self->vblank_sema, kTimeInterval_Infinity);
}

// Compiles a Copper program to display the null screen. The null screen shows
// nothing.
static errno_t create_null_copper_prog(CopperProgram* _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    CopperProgram* prog;
    const size_t instrCount = 1             // CLUT
            + 3                             // BPLCON0, BPLCON1, BPLCON2
            + 2 * NUM_HARDWARE_SPRITES      // SPRxDATy
            + 2                             // DIWSTART, DIWSTOP
            + 2                             // DDFSTART, DDFSTOP
            + 1                             // DMACON
            + 1;                            // COP_END

    err = CopperProgram_Create(instrCount, &prog);
    if (err == EOK) {
        CopperInstruction* ip = prog->entry;

        // DMACON
        *ip++ = COP_MOVE(DMACON, DMACONF_BPLEN | DMACONF_SPREN);


        // CLUT
        *ip++ = COP_MOVE(COLOR_BASE, 0);


        // BPLCONx
        *ip++ = COP_MOVE(BPLCON0, BPLCON0F_COLOR);
        *ip++ = COP_MOVE(BPLCON1, 0);
        *ip++ = COP_MOVE(BPLCON2, 0);


        // SPRxDATy
        for (int i = 0, r = SPRITE_BASE; i < NUM_HARDWARE_SPRITES; i++, r += 8) {
            *ip++ = COP_MOVE(r + SPR0DATA, 0);
            *ip++ = COP_MOVE(r + SPR0DATB, 0);
        }


        // DIWSTART / DIWSTOP
        *ip++ = COP_MOVE(DIWSTART, (DIW_NTSC_VSTART << 8) | DIW_NTSC_HSTART);
        *ip++ = COP_MOVE(DIWSTOP, (DIW_NTSC_VSTOP << 8) | DIW_NTSC_HSTOP);


        // DDFSTART / DDFSTOP
        *ip++ = COP_MOVE(DDFSTART, 0x0038);
        *ip++ = COP_MOVE(DDFSTOP, 0x00d0);


        // end instruction
        *ip = COP_END();
    }
    
    *pOutProg = prog;
    return err;
}

// Compiles a Copper program to display a non-interlaced screen or a single
// field of an interlaced screen.
static errno_t create_screen_copper_prog(Screen* _Nonnull scr, size_t instrCount, bool isLightPenEnabled, bool isOddField, CopperProgram* _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    CopperProgram* prog;
    
    err = CopperProgram_Create(instrCount, &prog);
    if (err == EOK) {
        CopperInstruction* ip = prog->entry;

        ip = Screen_MakeCopperProgram(scr, ip, isLightPenEnabled, isOddField);

        // end instruction
        *ip = COP_END();
    }
    
    *pOutProg = prog;
    return err;
}

// Creates the even and odd field Copper programs for the given screen. There will
// always be at least an odd field program. The even field program will only exist
// for an interlaced screen.
static errno_t create_field_copper_progs(Screen* _Nonnull scr, bool isLightPenEnabled, CopperProgram* _Nullable * _Nonnull pOutOddFieldProg, CopperProgram* _Nullable * _Nonnull pOutEvenFieldProg)
{
    decl_try_err();
    const size_t instrCount = Screen_CalcCopperProgramLength(scr) + 1;
    CopperProgram* oddFieldProg = NULL;
    CopperProgram* evenFieldProg = NULL;

    err = create_screen_copper_prog(scr, instrCount, isLightPenEnabled, true, &oddFieldProg);
    if (err == EOK && Screen_IsInterlaced(scr)) {
        err = create_screen_copper_prog(scr, instrCount, isLightPenEnabled, false, &evenFieldProg);
        if (err != EOK) {
            CopperProgram_Destroy(oddFieldProg);
            oddFieldProg = NULL;
        }
    }

    *pOutOddFieldProg = oddFieldProg;
    *pOutEvenFieldProg = evenFieldProg;
    return err;
}

// Sets the given screen as the current screen on the graphics driver. All graphics
// command apply to this new screen once this function has returned.
// \param scr the new screen
// \return the error code
static errno_t GraphicsDriver_SetCurrentScreen_Locked(GraphicsDriverRef _Nonnull _Locked self, Screen* _Nullable scr)
{
    decl_try_err();
    Screen* pOldScreen = self->screen;
    bool wasMouseCursorVisible = self->mousePainter.flags.isVisible;
    CopperProgram* oddFieldProg = NULL;
    CopperProgram* evenFieldProg = NULL;
    

    // Compile the Copper program(s) for the new screen
    if (scr) {
        err = create_field_copper_progs(scr, self->isLightPenEnabled, &oddFieldProg, &evenFieldProg);
    }
    else {
        err = create_null_copper_prog(&oddFieldProg);
    }
    if (err != EOK) {
        return err;
    }


    // Disassociate the mouse painter from the old screen (hides the mouse cursor)
    MousePainter_SetSurface(&self->mousePainter, NULL);


    // Update the display configuration.
    self->screen = scr;


    // Schedule the new Copper programs
    CopperScheduler_ScheduleProgram(&self->copperScheduler, oddFieldProg, evenFieldProg);
    

    // Wait for the vblank. Once we got a vblank we know that the DMA is no longer
    // accessing the old framebuffer
    GraphicsDriver_WaitForVerticalBlank_Locked(self);
    
    
    if (scr) {
        // Associate the mouse painter with the new screen
        MousePainter_SetSurface(&self->mousePainter, scr->surface);
        MousePainter_SetVisible(&self->mousePainter, wasMouseCursorVisible);
    }


    // Free the old screen
    Screen_Destroy(pOldScreen);

    return EOK;
}

errno_t GraphicsDriver_SetCurrentScreen(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr)
{
    Driver_Lock(self);
    const errno_t err = GraphicsDriver_SetCurrentScreen_Locked(self, scr);
    Driver_Unlock(self);
    return err;
}

Screen* _Nullable GraphicsDriver_GetCurrentScreen(GraphicsDriverRef _Nonnull self)
{
    Driver_Lock(self);
    Screen* scr = self->screen;
    Driver_Unlock(self);
    return scr;
}

// Triggers an update of the display so that it accurately reflects the current
// display configuration.
errno_t GraphicsDriver_UpdateDisplay(GraphicsDriverRef _Nonnull self)
{
    decl_try_err();

    Driver_Lock(self);
    Screen* scr = self->screen;

    if (scr && scr->flags.isNewCopperProgNeeded) {
        CopperProgram* oddFieldProg;
        CopperProgram* evenFieldProg;

        err = create_field_copper_progs(scr, self->isLightPenEnabled, &oddFieldProg, &evenFieldProg);
        if (err == EOK) {
            CopperScheduler_ScheduleProgram(&self->copperScheduler, oddFieldProg, evenFieldProg);
            scr->flags.isNewCopperProgNeeded = false;
        }
    }

    Driver_Unlock(self);
    return err;
}

Size GraphicsDriver_GetDisplaySize(GraphicsDriverRef _Nonnull self)
{
    Size siz;

    Driver_Lock(self);
    if (self->screen) {
        Screen_GetPixelSize(self->screen, &siz.width, &siz.height);
    }
    else {
        siz.width = 0;
        siz.height = 0;
    }
    Driver_Unlock(self);

    return siz;
}

// Enables / disables the h/v raster position latching triggered by a light pen.
void GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull self, bool enabled)
{
    Driver_Lock(self);
    if (self->isLightPenEnabled != enabled) {
        self->isLightPenEnabled = enabled;
        if (self->screen) {
            Screen_SetNeedsUpdate(self->screen);
        }
    }
    Driver_Unlock(self);
}

// Returns the current position of the light pen if the light pen triggered.
bool GraphicsDriver_GetLightPenPosition(GraphicsDriverRef _Nonnull self, int16_t* _Nonnull pPosX, int16_t* _Nonnull pPosY)
{
    CHIPSET_BASE_DECL(cp);
    bool r = false;
    
    Driver_Lock(self);

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
            r = true;
        }
    }

    Driver_Unlock(self);

    return r;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Screens
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_CreateScreen(GraphicsDriverRef _Nonnull self, const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, Screen* _Nullable * _Nonnull pOutScreen)
{
    Driver_Lock(self);
    const errno_t err = Screen_Create(pConfig, pixelFormat, self->nullSprite, pOutScreen);
    Driver_Unlock(self);
    return err;
}

errno_t GraphicsDriver_DestroyScreen(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr)
{
    Driver_Lock(self);
    if (scr) {
        Screen_Destroy(scr);
    }
    Driver_Unlock(self);
    return EOK;
}

const ScreenConfiguration* _Nonnull GraphicsDriver_GetScreenConfiguration(GraphicsDriverRef _Nonnull self, Screen* _Nonnull scr)
{
    Driver_Lock(self);
    const ScreenConfiguration* cfg = Screen_GetConfiguration(scr);
    Driver_Unlock(self);
    return cfg;
}

Size GraphicsDriver_GetScreenSize(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr)
{
    Size siz;

    Driver_Lock(self);
    if (scr) {
        Screen_GetPixelSize(scr, &siz.width, &siz.height);
    }
    else {
        siz.width = 0;
        siz.height = 0;
    }
    Driver_Unlock(self);

    return siz;
}

errno_t GraphicsDriver_LockScreenPixels(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, PixelAccess access, void* _Nonnull plane[8], size_t bytesPerRow[8], size_t* _Nonnull planeCount)
{
    decl_try_err();

    Driver_Lock(self);
    if (scr) {
        err = Screen_LockPixels(scr, access, plane, bytesPerRow, planeCount);
        if (err == EOK) {
            MousePainter_ShieldCursor(&self->mousePainter, Rect_Make(0, 0, scr->surface->width, scr->surface->height));
        }
    }
    else {
        err = ENOTSUP;
    }
    Driver_Unlock(self);
    return err;
}

errno_t GraphicsDriver_UnlockScreenPixels(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr)
{
    decl_try_err();

    Driver_Lock(self);
    if (scr) {
        MousePainter_UnshieldCursor(&self->mousePainter);
        err = Screen_UnlockPixels(scr);
    }
    else {
        err = ENOTSUP;
    }
    Driver_Unlock(self);
    return err;
}

// Writes the given RGB color to the color register at index idx
errno_t GraphicsDriver_SetCLUTEntry(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, size_t idx, RGBColor32 color)
{
    decl_try_err();

    Driver_Lock(self);
    if (scr) {
        err = Screen_SetCLUTEntry(scr, idx, color);
    }
    else {
        err = ENOTSUP;
    }
    Driver_Unlock(self);
    return err;
}

// Sets the contents of 'count' consecutive CLUT entries starting at index 'idx'
// to the colors in the array 'entries'.
errno_t GraphicsDriver_SetCLUTEntries(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
{
    decl_try_err();

    Driver_Lock(self);
    if (scr) {
        err = Screen_SetCLUTEntries(scr, idx, count, entries);
    }
    else {
        err = ENOTSUP;
    }
    Driver_Unlock(self);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprites
////////////////////////////////////////////////////////////////////////////////

// Acquires a hardware sprite
errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, int* _Nonnull pOutSpriteId)
{
    decl_try_err();

    Driver_Lock(self);
    if (scr) {
        err = Screen_AcquireSprite(scr, pPlanes, x, y, width, height, priority, pOutSpriteId);
    }
    else {
        err = ENOTSUP;
    }
    Driver_Unlock(self);
    return err;
}

// Relinquishes a hardware sprite
errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, int spriteId)
{
    decl_try_err();

    Driver_Lock(self);
    if (scr) {
        err = Screen_RelinquishSprite(scr, spriteId);
    }
    else {
        err = ENOTSUP;
    }
    Driver_Unlock(self);
    return err;
}

// Updates the position of a hardware sprite.
errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, int spriteId, int x, int y)
{
    decl_try_err();

    Driver_Lock(self);
    if (scr) {
        err = Screen_SetSpritePosition(scr, spriteId, x, y);
    }
    else {
        err = ENOTSUP;
    }
    Driver_Unlock(self);
    return err;
}

// Updates the visibility of a hardware sprite.
errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, Screen* _Nullable scr, int spriteId, bool isVisible)
{
    decl_try_err();

    Driver_Lock(self);
    if (scr) {
        err = Screen_SetSpriteVisible(scr, spriteId, isVisible);
    }
    else {
        err = ENOTSUP;
    }
    Driver_Unlock(self);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

void GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull self, const void* pBitmap, const void* pMask)
{
    Driver_Lock(self);
    MousePainter_SetCursor(&self->mousePainter, pBitmap, pMask);
    Driver_Unlock(self);
}

void GraphicsDriver_SetMouseCursorVisible(GraphicsDriverRef _Nonnull self, bool isVisible)
{
    Driver_Lock(self);
    MousePainter_SetVisible(&self->mousePainter, isVisible);
    Driver_Unlock(self);
}

void GraphicsDriver_SetMouseCursorHiddenUntilMove(GraphicsDriverRef _Nonnull self, bool flag)
{
    Driver_Lock(self);
    MousePainter_SetHiddenUntilMove(&self->mousePainter, flag);
    Driver_Unlock(self);
}

void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, Point loc)
{
    Driver_Lock(self);
    MousePainter_SetPosition(&self->mousePainter, loc);
    Driver_Unlock(self);
}

void GraphicsDriver_SetMouseCursorPositionFromInterruptContext(GraphicsDriverRef _Nonnull self, int16_t x, int16_t y)
{
    MousePainter_SetPosition_VerticalBlank(&self->mousePainter, x, y);
}


class_func_defs(GraphicsDriver, Driver,
override_func_def(onStart, GraphicsDriver, Driver)
);
