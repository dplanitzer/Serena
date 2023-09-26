//
//  GraphicsDriver.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"

extern const UInt16 gArrow_Plane0[];
extern const UInt16 gArrow_Plane1[];
extern const Int gArrow_Width;
extern const Int gArrow_Height;


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
// MARK: -
// MARK: Screen
////////////////////////////////////////////////////////////////////////////////

static ErrorCode Screen_CompileCopperPrograms(Screen* _Nonnull pScreen);


static void Screen_Destroy(Screen* _Nullable pScreen)
{
    if (pScreen) {
        CopperProgram_Destroy(pScreen->copperProgramEvenField);
        pScreen->copperProgramEvenField = NULL;
        
        CopperProgram_Destroy(pScreen->copperProgramOddField);
        pScreen->copperProgramOddField = NULL;
        
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
static ErrorCode Screen_Create(const VideoConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, const UInt16* _Nonnull pNullSprite, const UInt16* _Nonnull pSprite, Bool isLightPenEnabled, Screen* _Nullable * _Nonnull pOutScreen)
{
    decl_try_err();
    Screen* pScreen;
    
    try(kalloc_cleared(sizeof(Screen), (Byte**) &pScreen));
    
    pScreen->videoConfig = pConfig;
    pScreen->pixelFormat = pixelFormat;
    pScreen->nullSprite = pNullSprite;
    pScreen->mouseCursorSprite = pSprite;
    pScreen->isInterlaced = (pConfig->bplcon0 & BPLCON0_LACE) != 0;
    pScreen->isLightPenEnabled = isLightPenEnabled;

    
    // Allocate an appropriate framebuffer
    try(Surface_Create(pConfig->width, pConfig->height, pixelFormat, &pScreen->framebuffer));
    
    
    // Lock the new surface
    try(Surface_LockPixels(pScreen->framebuffer, kSurfaceAccess_Read|kSurfaceAccess_Write));
    
    
    // Compile the Copper program
    try(Screen_CompileCopperPrograms(pScreen));
    
    *pOutScreen = pScreen;
    return EOK;
    
catch:
    Screen_Destroy(pScreen);
    *pOutScreen = NULL;
    return err;
}

static ErrorCode Screen_CompileCopperPrograms(Screen* _Nonnull pScreen)
{
    decl_try_err();
    
    if (pScreen->isInterlaced) {
        try(CopperProgram_CreateScreenRefresh(pScreen, true, &pScreen->copperProgramOddField));
        try(CopperProgram_CreateScreenRefresh(pScreen, false, &pScreen->copperProgramEvenField));
    } else {
        try(CopperProgram_CreateScreenRefresh(pScreen, true, &pScreen->copperProgramOddField));
        pScreen->copperProgramEvenField = NULL;
    }

    return EOK;

catch:
    return err;
}

static ErrorCode Screen_SetLightPenEnabled(Screen* _Nonnull pScreen, Bool enabled)
{
    pScreen->isLightPenEnabled = enabled;
    return Screen_CompileCopperPrograms(pScreen);
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

    pDriver->mouse_cursor_width = gArrow_Width;
    pDriver->mouse_cursor_height = gArrow_Height;
    pDriver->mouse_cursor_hotspot_x = 1;
    pDriver->mouse_cursor_hotspot_y = 1;

    const Int nWords = 2 + 2*pDriver->mouse_cursor_height + 2;
    try(kalloc_options(sizeof(UInt16) * nWords, KALLOC_OPTION_UNIFIED, (Byte**) &pDriver->sprite_mouse));
    UInt16* sp = pDriver->sprite_mouse;

    *sp++ = (pConfig->diw_start_v << 8) | (pConfig->diw_start_h >> 1);
    *sp++ = (pConfig->diw_start_v + pDriver->mouse_cursor_height) << 8;
    for (Int i = 0; i < pDriver->mouse_cursor_height; i++) {
        *sp++ = gArrow_Plane0[i];
        *sp++ = gArrow_Plane1[i];
    }
    *sp++ = 0;
    *sp   = 0;
    

    // Initialize vblank tools
    CopperScheduler_Init(&pDriver->copperScheduler);
    Semaphore_Init(&pDriver->vblank_sema, 0);
    try(InterruptController_AddDirectInterruptHandler(
        gInterruptController,
        INTERRUPT_ID_VERTICAL_BLANK,
        INTERRUPT_HANDLER_PRIORITY_NORMAL,
        (InterruptHandler_Closure)GraphicsDriver_VerticalBlankInterruptHandler,
        (Byte*)pDriver, &pDriver->vb_irq_handler)
    );

    // Initialize the video config related stuff
    GraphicsDriver_SetCLUT(pDriver, &gDefaultColorTable);

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
        GraphicsDriver_StopVideoRefresh(pDriver);
        
        try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, pDriver->vb_irq_handler));
        pDriver->vb_irq_handler = 0;
        
        Semaphore_Deinit(&pDriver->vblank_sema);
        CopperScheduler_Deinit(&pDriver->copperScheduler);
        
        Screen_Destroy(pDriver->screen);
        pDriver->screen = NULL;

        kfree((Byte*)pDriver->sprite_mouse);
        pDriver->sprite_mouse = NULL;
        
        kfree((Byte*)pDriver->sprite_null);
        pDriver->sprite_null = NULL;
        
        kfree((Byte*)pDriver);
    }
}

void GraphicsDriver_VerticalBlankInterruptHandler(GraphicsDriverRef _Nonnull pDriver)
{
    CopperScheduler_Run(&pDriver->copperScheduler);
    Semaphore_ReleaseFromInterruptContext(&pDriver->vblank_sema);
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
    return pDriver->screen->framebuffer;
}

// Stops the video refresh circuitry
void GraphicsDriver_StopVideoRefresh(GraphicsDriverRef _Nonnull pDriver)
{
    CHIPSET_BASE_DECL(cp);

    *CHIPSET_REG_16(cp, DMACON) = (DMAF_COPPER | DMAF_RASTER | DMAF_SPRITE | DMAF_BLITTER);
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
    Screen* pOldScreen = pDriver->screen;

    // Allocate a new screen
    Screen* pNewScreen;
    try(Screen_Create(pConfig, pixelFormat, pDriver->sprite_null, pDriver->sprite_mouse, pDriver->is_light_pen_enabled, &pNewScreen));
    
    
    // Update the graphics device state.
    pDriver->screen = pNewScreen;
    
    // Turn video refresh back on and point it to the new copper program
    CopperScheduler_ScheduleProgram(&pDriver->copperScheduler, pNewScreen->copperProgramOddField, pNewScreen->copperProgramEvenField);
    
    // Wait for the vblank. Once we got a vblank we know that the DMA is no longer
    // accessing the old framebuffer
    try(GraphicsDriver_WaitForVerticalBlank(pDriver));
    
    
    // Free the old screen
    Screen_Destroy(pOldScreen);

    return EOK;

catch:
    return err;
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
    decl_try_err();

    if (pDriver->is_light_pen_enabled != enabled) {
        pDriver->is_light_pen_enabled = enabled;
        err = Screen_SetLightPenEnabled(pDriver->screen, enabled);
        if (err == EOK) {
            CopperScheduler_ScheduleProgram(&pDriver->copperScheduler, pDriver->screen->copperProgramOddField, pDriver->screen->copperProgramEvenField);
        }
    }

    return err;
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
            
            if (pDriver->screen->isInterlaced && posr0 < 0) {
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
    const UInt16 hshift = (pDriver->screen->videoConfig->spr_shift & 0xf0) >> 4;
    const UInt16 vshift = pDriver->screen->videoConfig->spr_shift & 0x0f;

    return Size_Make(pDriver->mouse_cursor_width << hshift, pDriver->mouse_cursor_height << vshift);
}

// Returns the offset of the hot spot inside the mouse cursor relative to the
// mouse cursor origin (top-left corner of the mouse cursor image).
Point GraphicsDriver_GetMouseCursorHotSpot(GraphicsDriverRef _Nonnull pDriver)
{
    const UInt16 hshift = (pDriver->screen->videoConfig->spr_shift & 0xf0) >> 4;
    const UInt16 vshift = pDriver->screen->videoConfig->spr_shift & 0x0f;
    
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
    const UInt16 hshift = (pDriver->screen->videoConfig->spr_shift & 0xf0) >> 4;
    const UInt16 vshift = pDriver->screen->videoConfig->spr_shift & 0x0f;
    const UInt16 hstart = (xPos >> hshift) + pDriver->screen->videoConfig->diw_start_h;
    const UInt16 vstart = (yPos >> vshift) + pDriver->screen->videoConfig->diw_start_v;
    const UInt16 vstop = vstart + pDriver->mouse_cursor_height;
    const UInt16 sprxpos = ((vstart & 0xff) << 8) | ((hstart & 0x1fe) >> 1);
    const UInt16 sprxctl = ((vstop & 0xff) << 8) | (((vstart >> 8) & 0x01) << 2) | (((vstop >> 8) & 0x01) << 1) | (hstart & 0x001);

    pDriver->sprite_mouse[0] = sprxpos;
    pDriver->sprite_mouse[1] = sprxctl;
}
