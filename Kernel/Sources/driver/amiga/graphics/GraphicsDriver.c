//
//  GraphicsDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"
#include <System/HID.h>


// Creates a graphics driver instance which manages the on-board video hardware.
// We assume that video is turned off at the time this function is called and
// video remains turned off until a screen has been created and is made the
// current screen.
errno_t GraphicsDriver_Create(DriverRef _Nullable parent, GraphicsDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    GraphicsDriverRef self;
    
    try(Driver_Create(class(GraphicsDriver), 0, parent, (DriverRef*)&self));
    self->nextSurfaceId = 1;
    self->nextScreenId = 1;


    // Allocate the Copper tools
    CopperScheduler_Init(&self->copperScheduler);


    // Allocate the null and mouse cursor sprite
    try(Sprite_Create(MAX_SPRITE_WIDTH, 0, kPixelFormat_RGB_Indexed2, &self->nullSprite));
    try(Sprite_Create(kMouseCursor_Width, kMouseCursor_Height, kMouseCursor_PixelFormat, &self->mouseCursor));


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
    Semaphore_RelinquishFromInterruptContext(&self->vblank_sema);
}

static errno_t GraphicsDriver_onStart(DriverRef _Nonnull _Locked self)
{
    DriverEntry de;
    de.name = "fb";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = FilePermissions_MakeFromOctal(0666);
    de.arg = 0;

    return Driver_Publish(self, &de);
}

errno_t GraphicsDriver_ioctl(GraphicsDriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kFBCommand_CreateSurface: {
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const PixelFormat fmt = va_arg(ap, PixelFormat);
            int* hnd = va_arg(ap, int*);

            return GraphicsDriver_CreateSurface(self, width, height, fmt, hnd);
        }

        case kFBCommand_DestroySurface: {
            int hnd = va_arg(ap, int);

            return GraphicsDriver_DestroySurface(self, hnd);
        }

        case kFBCommand_GetSurfaceInfo: {
            int hnd = va_arg(ap, int);
            SurfaceInfo* si = va_arg(ap, SurfaceInfo*);

            return GraphicsDriver_GetSurfaceInfo(self, hnd, si);
        }

        case kFBCommand_MapSurface: {
            int hnd = va_arg(ap, int);
            MapPixels mode = va_arg(ap, MapPixels);
            SurfaceMapping* sm = va_arg(ap, SurfaceMapping*);

            return GraphicsDriver_MapSurface(self, hnd, mode, sm);
        }

        case kFBCommand_UnmapSurface: {
            const int hnd = va_arg(ap, int);

            return GraphicsDriver_UnmapSurface(self, hnd);
        }


        case kFBCommand_CreateScreen: {
            const VideoConfiguration* vc = va_arg(ap, const VideoConfiguration*);
            const int sid = va_arg(ap, int);
            int* hnd = va_arg(ap, int*);

            return GraphicsDriver_CreateScreen(self, vc, sid, hnd);
        }

        case kFBCommand_DestroyScreen: {
            const int hnd = va_arg(ap, int);

            return GraphicsDriver_DestroyScreen(self, hnd);
        }

        case kFBCommand_SetCLUTEntries: {
            const int hnd = va_arg(ap, int);
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            const RGBColor32* colors = va_arg(ap, const RGBColor32*);

            return GraphicsDriver_SetCLUTEntries(self, hnd, idx, count, colors);
        }

        case kFBCommand_AcquireSprite: {
            const int hnd = va_arg(ap, int);
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const PixelFormat fmt = va_arg(ap, PixelFormat);
            const int pri = va_arg(ap, int);
            int* sid = va_arg(ap, int*);

            return GraphicsDriver_AcquireSprite(self, hnd, width, height, fmt, pri, sid);
        }

        case kFBCommand_RelinquishSprite: {
            const int hnd = va_arg(ap, int);

            return GraphicsDriver_RelinquishSprite(self, hnd);
        }

        case kFBCommand_SetSpritePixels: {
            const int hnd = va_arg(ap, int);
            const uint16_t** planes = va_arg(ap, const uint16_t**);

            return GraphicsDriver_SetSpritePixels(self, hnd, planes);
        }

        case kFBCommand_SetSpritePosition: {
            const int hnd = va_arg(ap, int);
            const int x = va_arg(ap, int);
            const int y = va_arg(ap, int);

            return GraphicsDriver_SetSpritePosition(self, hnd, x, y);
        }

        case kFBCommand_SetSpriteVisible: {
            const int hnd = va_arg(ap, int);
            const bool flag = va_arg(ap, bool);

            return GraphicsDriver_SetSpriteVisible(self, hnd, flag);
        }


        case kFBCommand_SetCurrentScreen: {
            const int hnd = va_arg(ap, int);

            return GraphicsDriver_SetCurrentScreen(self, hnd);
        }

        case kFBCommand_GetCurrentScreen:
            return GraphicsDriver_GetCurrentScreen(self);

        case kFBCommand_UpdateDisplay:
            return GraphicsDriver_UpdateDisplay(self);


        case kFBCommand_GetVideoConfigurationRange: {
            VideoConfigurationRange* vcr = va_arg(ap, VideoConfigurationRange*);
            const size_t bufSize = va_arg(ap, size_t);
            size_t* iter = va_arg(ap, size_t*);
            
            return GraphicsDriver_GetVideoConfigurationRange(self, vcr, bufSize, iter);
        }


        default:
            return super_n(ioctl, Driver, GraphicsDriver, self, pChannel, cmd, ap);
    }
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
static errno_t create_screen_copper_prog(Screen* _Nonnull scr, size_t instrCount, Sprite* _Nullable mouseCursor, bool isLightPenEnabled, bool isOddField, CopperProgram* _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    CopperProgram* prog;
    
    err = CopperProgram_Create(instrCount, &prog);
    if (err == EOK) {
        CopperInstruction* ip = prog->entry;

        ip = Screen_MakeCopperProgram(scr, ip, mouseCursor, isLightPenEnabled, isOddField);

        // end instruction
        *ip = COP_END();
    }
    
    *pOutProg = prog;
    return err;
}

// Creates the even and odd field Copper programs for the given screen. There will
// always be at least an odd field program. The even field program will only exist
// for an interlaced screen.
static errno_t create_field_copper_progs(Screen* _Nonnull scr, Sprite* _Nullable mouseCursor, bool isLightPenEnabled, CopperProgram* _Nullable * _Nonnull pOutOddFieldProg, CopperProgram* _Nullable * _Nonnull pOutEvenFieldProg)
{
    decl_try_err();
    const size_t instrCount = Screen_CalcCopperProgramLength(scr) + 1;
    CopperProgram* oddFieldProg = NULL;
    CopperProgram* evenFieldProg = NULL;

    err = create_screen_copper_prog(scr, instrCount, mouseCursor, isLightPenEnabled, true, &oddFieldProg);
    if (err == EOK && Screen_IsInterlaced(scr)) {
        err = create_screen_copper_prog(scr, instrCount, mouseCursor, isLightPenEnabled, false, &evenFieldProg);
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
    CopperProgram* oddFieldProg = NULL;
    CopperProgram* evenFieldProg = NULL;
    

    // Can't show a screen that's already being shown
    if (scr && Screen_IsVisible(scr)) {
        return EBUSY;
    }


    // Compile the Copper program(s) for the new screen
    if (scr) {
        err = create_field_copper_progs(scr, (self->flags.mouseCursorEnabled) ? self->mouseCursor : NULL, self->flags.isLightPenEnabled, &oddFieldProg, &evenFieldProg);
    }
    else {
        err = create_null_copper_prog(&oddFieldProg);
    }
    if (err != EOK) {
        return err;
    }


    // Update the display configuration.
    self->screen = scr;
    if (scr) {
        Screen_SetVisible(scr, true);
        self->mouseCursorRectX = scr->hDiwStart;
        self->mouseCursorRectY = scr->vDiwStart;
        self->mouseCursorScaleX = scr->hSprScale;
        self->mouseCursorScaleY = scr->vSprScale;
    } 
    else {
        self->mouseCursorRectX = 0;
        self->mouseCursorRectY = 0;
        self->mouseCursorScaleX = 0;
        self->mouseCursorScaleY = 0;
    } 


    // Schedule the new Copper programs
    CopperScheduler_ScheduleProgram(&self->copperScheduler, oddFieldProg, evenFieldProg);
    

    // Wait for the vblank. Once we got a vblank we know that the DMA is no longer
    // accessing the old framebuffer
    GraphicsDriver_WaitForVerticalBlank_Locked(self);


    // Free the old screen
    if (pOldScreen) {
        Screen_SetVisible(pOldScreen, false);
        Screen_Destroy(pOldScreen);
    }

    return EOK;
}

errno_t GraphicsDriver_SetCurrentScreen(GraphicsDriverRef _Nonnull self, int screenId)
{
    Driver_Lock(self);
    decl_try_err();
    Screen* scr = _GraphicsDriver_GetScreenForId(self, screenId);
    if (scr || screenId == 0) {
        err = GraphicsDriver_SetCurrentScreen_Locked(self, scr);
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return err;
}

int GraphicsDriver_GetCurrentScreen(GraphicsDriverRef _Nonnull self)
{
    Driver_Lock(self);
    int id = (self->screen) ? Surface_GetId(self->screen) : 0;
    Driver_Unlock(self);
    return id;
}

// Triggers an update of the display so that it accurately reflects the current
// display configuration.
errno_t GraphicsDriver_UpdateDisplay(GraphicsDriverRef _Nonnull self)
{
    decl_try_err();

    Driver_Lock(self);
    Screen* scr = self->screen;

    if (scr && (self->flags.isNewCopperProgNeeded || Screen_NeedsUpdate(scr))) {
        CopperProgram* oddFieldProg;
        CopperProgram* evenFieldProg;

        err = create_field_copper_progs(scr, (self->flags.mouseCursorEnabled) ? self->mouseCursor : NULL, self->flags.isLightPenEnabled, &oddFieldProg, &evenFieldProg);
        if (err == EOK) {
            CopperScheduler_ScheduleProgram(&self->copperScheduler, oddFieldProg, evenFieldProg);
            scr->flags &= ~kScreenFlag_IsNewCopperProgNeeded;
            self->flags.isNewCopperProgNeeded = 0;
        }
    }

    Driver_Unlock(self);
    return err;
}

void GraphicsDriver_GetDisplaySize(GraphicsDriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    Driver_Lock(self);
    if (self->screen) {
        Screen_GetPixelSize(self->screen, pOutWidth, pOutHeight);
    }
    else {
        *pOutWidth = 0;
        *pOutHeight = 0;
    }
    Driver_Unlock(self);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Surfaces
////////////////////////////////////////////////////////////////////////////////

static int _GraphicsDriver_GetNewSurfaceId(GraphicsDriverRef _Nonnull _Locked self)
{
    bool hasCollision = true;
    int id;

    while (hasCollision) {
        hasCollision = false;
        id = self->nextSurfaceId++;

        List_ForEach(&self->surfaces, Surface,
            if (Surface_GetId(pCurNode) == id) {
                hasCollision = true;
                break;
            }
        );
    }
    return id;
}

static Surface* _Nullable _GraphicsDriver_GetSurfaceForId(GraphicsDriverRef _Nonnull self, int id)
{
    List_ForEach(&self->surfaces, Surface,
        if (Surface_GetId(pCurNode) == id) {
            return pCurNode;
        }
    );
    return NULL;
}

errno_t GraphicsDriver_CreateSurface(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int* _Nonnull pOutId)
{
    Surface* srf;

    Driver_Lock(self);
    const errno_t err = Surface_Create(_GraphicsDriver_GetNewSurfaceId(self), width, height, pixelFormat, &srf);
    if (err == EOK) {
        List_InsertBeforeFirst(&self->surfaces, &srf->chain);
        *pOutId = Surface_GetId(srf);
    }
    Driver_Unlock(self);
    return err;
}

errno_t GraphicsDriver_DestroySurface(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    Driver_Lock(self);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);

    if (srf) {
        if (!Surface_IsUsed(srf)) {
            Surface_Destroy(srf);
        }
        else {
            err = EBUSY;
        }
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return err;
}

errno_t GraphicsDriver_GetSurfaceInfo(GraphicsDriverRef _Nonnull self, int id, SurfaceInfo* _Nonnull pOutInfo)
{
    decl_try_err();

    Driver_Lock(self);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);

    if (srf) {
        pOutInfo->width = Surface_GetWidth(srf);
        pOutInfo->height = Surface_GetHeight(srf);
        pOutInfo->pixelFormat = Surface_GetPixelFormat(srf);
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return EOK;
}

errno_t GraphicsDriver_MapSurface(GraphicsDriverRef _Nonnull self, int id, MapPixels mode, SurfaceMapping* _Nonnull pOutMapping)
{
    decl_try_err();

    Driver_Lock(self);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);
    if (srf) {
        err = Surface_Map(srf, mode, pOutMapping);
    }
    else {
        err = ENOTSUP;
    }
    Driver_Unlock(self);
    return err;
}

errno_t GraphicsDriver_UnmapSurface(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    Driver_Lock(self);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);
    if (srf) {
        err = Surface_Unmap(srf);
    }
    else {
        err = ENOTSUP;
    }
    Driver_Unlock(self);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Screens
////////////////////////////////////////////////////////////////////////////////

static int _GraphicsDriver_GetNewScreenId(GraphicsDriverRef _Nonnull _Locked self)
{
    bool hasCollision = true;
    int id;

    while (hasCollision) {
        hasCollision = false;
        id = self->nextScreenId++;

        List_ForEach(&self->screens, Screen,
            if (Screen_GetId(pCurNode) == id) {
                hasCollision = true;
                break;
            }
        );
    }
    return id;
}

static Screen* _Nullable _GraphicsDriver_GetScreenForId(GraphicsDriverRef _Nonnull self, int id)
{
    List_ForEach(&self->screens, Screen,
        if (Screen_GetId(pCurNode) == id) {
            return pCurNode;
        }
    );
    return NULL;
}

errno_t GraphicsDriver_CreateScreen(GraphicsDriverRef _Nonnull self, const VideoConfiguration* _Nonnull vidCfg, int surfaceId, int* _Nonnull pOutId)
{
    Driver_Lock(self);

    decl_try_err();
    Screen* scr;
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, surfaceId);
    
    if (srf) {
        err = VideoConfiguration_Validate(vidCfg, Surface_GetPixelFormat(srf));
        if (err == EOK) {
            err = Screen_Create(_GraphicsDriver_GetNewScreenId(self), vidCfg, srf, self->nullSprite, &scr);
            if (err == EOK) {
                List_InsertBeforeFirst(&self->screens, &scr->chain);
                *pOutId = Screen_GetId(scr);
            }
        }
    }
    else {
        err = EINVAL;
    }

    Driver_Unlock(self);
    return err;
}

errno_t GraphicsDriver_DestroyScreen(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    Driver_Lock(self);
    Screen* scr = _GraphicsDriver_GetScreenForId(self, id);
    if (scr) {
        if (!Screen_IsVisible(scr)) {
            Screen_Destroy(scr);
        }
        else {
            err = EBUSY;
        }
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return err;
}

errno_t GraphicsDriver_GetVideoConfiguration(GraphicsDriverRef _Nonnull self, int id, VideoConfiguration* _Nonnull pOutVidConfig)
{
    Driver_Lock(self);
    decl_try_err();
    Screen* scr = _GraphicsDriver_GetScreenForId(self, id);
    if (scr) {
        *pOutVidConfig = *Screen_GetVideoConfiguration(scr);
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return err;
}

// Writes the given RGB color to the color register at index idx
errno_t GraphicsDriver_SetCLUTEntry(GraphicsDriverRef _Nonnull self, int id, size_t idx, RGBColor32 color)
{
    decl_try_err();

    Driver_Lock(self);
    Screen* scr = _GraphicsDriver_GetScreenForId(self, id);
    if (scr) {
        err = Screen_SetCLUTEntry(scr, idx, color);
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return err;
}

// Sets the contents of 'count' consecutive CLUT entries starting at index 'idx'
// to the colors in the array 'entries'.
errno_t GraphicsDriver_SetCLUTEntries(GraphicsDriverRef _Nonnull self, int id, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
{
    decl_try_err();

    Driver_Lock(self);
    Screen* scr = _GraphicsDriver_GetScreenForId(self, id);
    if (scr) {
        err = Screen_SetCLUTEntries(scr, idx, count, entries);
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprites
////////////////////////////////////////////////////////////////////////////////

#define MAKE_SPRITE_ID(__scrId, __sprIdx) \
(((__scrId) << 3) | (__sprIdx))

#define GET_SPRITE_IDX(__sprId) \
((__sprId) & 0x07)

#define GET_SCREEN_ID(__sprId) \
((__sprId) >> 3)


// Acquires a hardware sprite
errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull self, int screenId, int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutSpriteId)
{
    decl_try_err();

    Driver_Lock(self);
    Screen* scr = _GraphicsDriver_GetScreenForId(self, screenId);
    if (scr) {
        int sprIdx;

        err = Screen_AcquireSprite(scr, width, height, pixelFormat, priority, &sprIdx);
        *pOutSpriteId = (err == EOK) ? MAKE_SPRITE_ID(scr->id, sprIdx) : 0;
    }
    else {
        err = EINVAL;
        *pOutSpriteId = 0;
    }
    Driver_Unlock(self);
    return err;
}

// Relinquishes a hardware sprite
errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull self, int spriteId)
{
    decl_try_err();

    if (spriteId == 0) {
        return EOK;
    }

    Driver_Lock(self);
    Screen* scr = _GraphicsDriver_GetScreenForId(self, GET_SCREEN_ID(spriteId));
    if (scr) {
        err = Screen_RelinquishSprite(scr, GET_SPRITE_IDX(spriteId));
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return err;
}

errno_t GraphicsDriver_SetSpritePixels(GraphicsDriverRef _Nonnull self, int spriteId, const uint16_t* _Nonnull planes[2])
{
    decl_try_err();

    Driver_Lock(self);
    Screen* scr = _GraphicsDriver_GetScreenForId(self, GET_SCREEN_ID(spriteId));
    if (scr) {
        err = Screen_SetSpritePixels(scr, GET_SPRITE_IDX(spriteId), planes);
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return err;
}

// Updates the position of a hardware sprite.
errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, int spriteId, int x, int y)
{
    decl_try_err();

    Driver_Lock(self);
    Screen* scr = _GraphicsDriver_GetScreenForId(self, GET_SCREEN_ID(spriteId));
    if (scr) {
        err = Screen_SetSpritePosition(scr, GET_SPRITE_IDX(spriteId), x, y);
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return err;
}

// Updates the visibility of a hardware sprite.
errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, int spriteId, bool isVisible)
{
    decl_try_err();

    Driver_Lock(self);
    Screen* scr = _GraphicsDriver_GetScreenForId(self, GET_SCREEN_ID(spriteId));
    if (scr) {
        err = Screen_SetSpriteVisible(scr, GET_SPRITE_IDX(spriteId), isVisible);
    }
    else {
        err = EINVAL;
    }
    Driver_Unlock(self);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Light Pen
////////////////////////////////////////////////////////////////////////////////

// Enables / disables the h/v raster position latching triggered by a light pen.
void GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull self, bool enabled)
{
    Driver_Lock(self);
    if (self->flags.isLightPenEnabled != enabled) {
        self->flags.isLightPenEnabled = enabled;
        self->flags.isNewCopperProgNeeded = 1;
    }
    Driver_Unlock(self);
}

// Returns the current position of the light pen if the light pen triggered.
bool GraphicsDriver_GetLightPenPositionFromInterruptContext(GraphicsDriverRef _Nonnull self, int16_t* _Nonnull pPosX, int16_t* _Nonnull pPosY)
{
    CHIPSET_BASE_DECL(cp);
    bool r = false;

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

    return r;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull self, const uint16_t* _Nullable planes[2], int width, int height, PixelFormat pixelFormat)
{
    if (width != kMouseCursor_Width || height != kMouseCursor_Height || pixelFormat != kMouseCursor_PixelFormat) {
        return ENOTSUP;
    }

    Driver_Lock(self);
    Sprite_SetPixels(self->mouseCursor, planes);
    self->flags.isNewCopperProgNeeded = 1;
    Driver_Unlock(self);
    return EOK;
}

// Sets the position of the mouse cursor. Note that the mouse cursor is only
// visible as long as at least some part of it is inside the visible display
// area. Additionally this API guarantees that the mouse cursor will be hidden
// if either 'x' or 'y' is == INT_MIN
void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, int x, int y)
{
    Driver_Lock(self);
    GraphicsDriver_SetMouseCursorPositionFromInterruptContext(self, x, y);
    Driver_Unlock(self);
}

void GraphicsDriver_SetMouseCursorPositionFromInterruptContext(GraphicsDriverRef _Nonnull self, int x, int y)
{
    const int16_t x16 = __max(__min(x, INT16_MAX), INT16_MIN);
    const int16_t y16 = __max(__min(y, INT16_MAX), INT16_MIN);
    const int16_t sprX = self->mouseCursorRectX - 1 + (x16 >> self->mouseCursorScaleX);
    const int16_t sprY = self->mouseCursorRectY + (y16 >> self->mouseCursorScaleY);

    Sprite_SetPosition(self->mouseCursor, sprX, sprY);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_GetVideoConfigurationRange(GraphicsDriverRef _Nonnull self, VideoConfigurationRange* _Nonnull config, size_t bufSize, size_t* _Nonnull pIter)
{
    Driver_Lock(self);
    const errno_t err = VideoConfiguration_GetNext(config, bufSize, pIter);
    Driver_Unlock(self);
    return err;
}


class_func_defs(GraphicsDriver, Driver,
override_func_def(onStart, GraphicsDriver, Driver)
override_func_def(ioctl, GraphicsDriver, Driver)
);
