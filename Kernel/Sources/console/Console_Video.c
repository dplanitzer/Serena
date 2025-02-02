//
//  Console_Video.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"


static const RGBColor32 gANSIColors[8] = {
    0xff000000,     // Black
    0xffff0000,     // Red
    0xff00ff00,     // Green
    0xffffff00,     // Yellow
    0xff0000ff,     // Blue
    0xffff00ff,     // Magenta
    0xff00ffff,     // Cyan
    0xffffffff,     // White
};

#define CURSOR_BLINKER_TAG  ((uintptr_t)0x1010)


// Initializes the video subsystem
errno_t Console_InitVideo(ConsoleRef _Nonnull self)
{
    decl_try_err();

    // Create a suitable screen
    const ScreenConfiguration* vidCfg;
    if (chipset_is_ntsc()) {
        vidCfg = &kScreenConfig_NTSC_640_200_60;
        //vidCfg = &kScreenConfig_NTSC_640_400_30;
    } else {
        vidCfg = &kScreenConfig_PAL_640_256_50;
        //vidCfg = &kScreenConfig_PAL_640_512_25;
    }
    const int pixelsWidth = ScreenConfiguration_GetPixelWidth(vidCfg);
    const int pixelsHeight = ScreenConfiguration_GetPixelHeight(vidCfg);

    try(GraphicsDriver_CreateSurface(self->gdevice, pixelsWidth, pixelsHeight, kPixelFormat_RGB_Indexed3, &self->surface));
    try(GraphicsDriver_CreateScreen(self->gdevice, vidCfg, self->surface, &self->screen));


    // Make our screen the current screen
    try(GraphicsDriver_SetCurrentScreen(self->gdevice, self->screen));


    // Install an ANSI color table
    GraphicsDriver_SetCLUTEntries(self->gdevice, self->screen, 0, sizeof(gANSIColors), gANSIColors);


    // Get the framebuffer size
    self->pixelsWidth = pixelsWidth;
    self->pixelsHeight = pixelsHeight;


    // Allocate the text cursor (sprite)
    const bool isLace = ScreenConfiguration_IsInterlaced(vidCfg) ? true : false;
    const uint16_t* textCursorPlanes[2];
    textCursorPlanes[0] = (isLace) ? &gBlock4x4_Plane0[0] : &gBlock4x8_Plane0[0];
    textCursorPlanes[1] = (isLace) ? &gBlock4x4_Plane0[1] : &gBlock4x8_Plane0[1];
    const int textCursorWidth = (isLace) ? gBlock4x4_Width : gBlock4x8_Width;
    const int textCursorHeight = (isLace) ? gBlock4x4_Height : gBlock4x8_Height;
    try(GraphicsDriver_AcquireSprite(self->gdevice, self->screen, textCursorPlanes, 0, 0, textCursorWidth, textCursorHeight, 0, &self->textCursor));
    self->flags.isTextCursorVisible = false;


    // Allocate the text cursor blinking timer
    self->flags.isTextCursorBlinkerEnabled = false;
    self->flags.isTextCursorOn = false;
    self->flags.isTextCursorSingleCycleOn = false;

    try(GraphicsDriver_UpdateDisplay(self->gdevice));
    try(GraphicsDriver_MapSurface(self->gdevice, self->surface, kMapPixels_ReadWrite, &self->pixels));

catch:
    return err;
}

// Deinitializes the video output subsystem
void Console_DeinitVideo(ConsoleRef _Nonnull self)
{
    GraphicsDriver_UnmapSurface(self->gdevice, self->surface);

    GraphicsDriver_SetCurrentScreen(self->gdevice, NULL);

    GraphicsDriver_RelinquishSprite(self->gdevice, self->screen, self->textCursor);
    GraphicsDriver_DestroyScreen(self->gdevice, self->screen);
    GraphicsDriver_DestroySurface(self->gdevice, self->surface);
    
    DispatchQueue_RemoveByTag(self->dispatchQueue, CURSOR_BLINKER_TAG);
}


// Sets the console's foreground color to the given color
void Console_SetForegroundColor_Locked(ConsoleRef _Nonnull self, Color color)
{
    assert(color.tag == kColorType_Index);
    self->foregroundColor = color;

    // Sync up the sprite color registers with the selected foreground color
    GraphicsDriver_SetCLUTEntry(self->gdevice, self->screen, 17, gANSIColors[color.u.index]);
    GraphicsDriver_SetCLUTEntry(self->gdevice, self->screen, 18, gANSIColors[color.u.index]);
    GraphicsDriver_SetCLUTEntry(self->gdevice, self->screen, 19, gANSIColors[color.u.index]);
    GraphicsDriver_UpdateDisplay(self->gdevice);
}

// Sets the console's background color to the given color
void Console_SetBackgroundColor_Locked(ConsoleRef _Nonnull self, Color color)
{
    assert(color.tag == kColorType_Index);
    self->backgroundColor = color;
}


void Console_OnTextCursorBlink(ConsoleRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    
    self->flags.isTextCursorOn = !self->flags.isTextCursorOn;
    if (self->flags.isTextCursorVisible) {
        GraphicsDriver_SetSpriteVisible(self->gdevice, self->screen, self->textCursor, self->flags.isTextCursorOn || self->flags.isTextCursorSingleCycleOn);
    }
    self->flags.isTextCursorSingleCycleOn = false;

    Lock_Unlock(&self->lock);
}

static void Console_UpdateCursorVisibilityAndRestartBlinking_Locked(ConsoleRef _Nonnull self)
{
    if (self->flags.isTextCursorVisible) {
        // Changing the visibility to on should restart the blinking timer if
        // blinking is on too so that we always start out with a cursor-on phase
        DispatchQueue_RemoveByTag(self->dispatchQueue, CURSOR_BLINKER_TAG);
        GraphicsDriver_SetSpriteVisible(self->gdevice, self->screen, self->textCursor, true);
        self->flags.isTextCursorOn = false;
        self->flags.isTextCursorSingleCycleOn = false;

        if (self->flags.isTextCursorBlinkerEnabled) {
            try_bang(DispatchQueue_DispatchAsyncPeriodically(self->dispatchQueue, 
                kTimeInterval_Zero,
                TimeInterval_MakeMilliseconds(500),
                (VoidFunc_1)Console_OnTextCursorBlink,
                self,
                CURSOR_BLINKER_TAG));
        }
    } else {
        // Make sure that the text cursor and blinker are off
        DispatchQueue_RemoveByTag(self->dispatchQueue, CURSOR_BLINKER_TAG);
        GraphicsDriver_SetSpriteVisible(self->gdevice, self->screen, self->textCursor, false);
        self->flags.isTextCursorOn = false;
        self->flags.isTextCursorSingleCycleOn = false;
    }
}

void Console_SetCursorBlinkingEnabled_Locked(ConsoleRef _Nonnull self, bool isEnabled)
{
    if (self->flags.isTextCursorBlinkerEnabled != isEnabled) {
        self->flags.isTextCursorBlinkerEnabled = isEnabled;
        Console_UpdateCursorVisibilityAndRestartBlinking_Locked(self);
    }
}

void Console_SetCursorVisible_Locked(ConsoleRef _Nonnull self, bool isVisible)
{
    if (self->flags.isTextCursorVisible != isVisible) {
        self->flags.isTextCursorVisible = isVisible;
        Console_UpdateCursorVisibilityAndRestartBlinking_Locked(self);
    }
}

void Console_CursorDidMove_Locked(ConsoleRef _Nonnull self)
{
    GraphicsDriver_SetSpritePosition(self->gdevice, self->screen, self->textCursor, self->x * self->characterWidth, self->y * self->lineHeight);
    // Temporarily force the cursor to be visible, but without changing the text
    // cursor visibility state officially. We just want to make sure that the
    // cursor is on when the user types a character. This however should not
    // change anything about the blinking phase and frequency.
    if (!self->flags.isTextCursorSingleCycleOn && !self->flags.isTextCursorOn && self->flags.isTextCursorBlinkerEnabled && self->flags.isTextCursorVisible) {
        self->flags.isTextCursorSingleCycleOn = true;
        GraphicsDriver_SetSpriteVisible(self->gdevice, self->screen, self->textCursor, true);
    }
}


void Console_BeginDrawing_Locked(ConsoleRef _Nonnull self)
{
    GraphicsDriver_ShieldMouseCursor(self->gdevice, 0, 0, INT_MAX, INT_MAX);
}

void Console_EndDrawing_Locked(ConsoleRef _Nonnull self)
{
    GraphicsDriver_UnshieldMouseCursor(self->gdevice);
}

static void Console_DrawGlyphBitmap_Locked(ConsoleRef _Nonnull self, const uint8_t* _Nonnull pSrc, int xc, int yc, const Color* _Nonnull fgColor, const Color* _Nonnull bgColor)
{
    for (size_t p = 0; p < self->pixels.planeCount; p++) {
        const int_fast8_t fgOne = fgColor->u.index & (1 << p);
        const int_fast8_t bgOne = bgColor->u.index & (1 << p);
        register const size_t bytesPerRow = self->pixels.bytesPerRow[p];
        register const uint8_t* sp = pSrc;
        register uint8_t* dp = (uint8_t*)self->pixels.plane[p] + (yc << 3) * bytesPerRow + xc;

        if (fgOne && !bgOne) {
            *dp = *sp++;    dp += bytesPerRow;
            *dp = *sp++;    dp += bytesPerRow;
            *dp = *sp++;    dp += bytesPerRow;
            *dp = *sp++;    dp += bytesPerRow;
            *dp = *sp++;    dp += bytesPerRow;
            *dp = *sp++;    dp += bytesPerRow;
            *dp = *sp++;    dp += bytesPerRow;
            *dp = *sp++;    dp += bytesPerRow;
        }
        else if (!fgOne && bgOne) {
            *dp = ~(*sp++); dp += bytesPerRow;
            *dp = ~(*sp++); dp += bytesPerRow;
            *dp = ~(*sp++); dp += bytesPerRow;
            *dp = ~(*sp++); dp += bytesPerRow;
            *dp = ~(*sp++); dp += bytesPerRow;
            *dp = ~(*sp++); dp += bytesPerRow;
            *dp = ~(*sp++); dp += bytesPerRow;
            *dp = ~(*sp++); dp += bytesPerRow;
        }
        else if(fgOne && bgOne) {
            *dp = 0xff;     dp += bytesPerRow;
            *dp = 0xff;     dp += bytesPerRow;
            *dp = 0xff;     dp += bytesPerRow;
            *dp = 0xff;     dp += bytesPerRow;
            *dp = 0xff;     dp += bytesPerRow;
            *dp = 0xff;     dp += bytesPerRow;
            *dp = 0xff;     dp += bytesPerRow;
            *dp = 0xff;     dp += bytesPerRow;
        }
        else {
            *dp = 0x00;     dp += bytesPerRow;
            *dp = 0x00;     dp += bytesPerRow;
            *dp = 0x00;     dp += bytesPerRow;
            *dp = 0x00;     dp += bytesPerRow;
            *dp = 0x00;     dp += bytesPerRow;
            *dp = 0x00;     dp += bytesPerRow;
            *dp = 0x00;     dp += bytesPerRow;
            *dp = 0x00;     dp += bytesPerRow;
        }
    }
}

void Console_DrawGlyph_Locked(ConsoleRef _Nonnull self, char glyph, int xc, int yc)
{
    const int maxX = self->pixelsWidth >> 3;
    const int maxY = self->pixelsHeight >> 3;
    
    if (xc >= 0 && yc >= 0 && xc < maxX && yc < maxY) {
        if (self->characterRendition.isHidden) {
            glyph = ' ';
        }

        const Color* fgColor = (self->characterRendition.isReverse) ? &self->backgroundColor : &self->foregroundColor;
        const Color* bgColor = (self->characterRendition.isReverse) ? &self->foregroundColor : &self->backgroundColor;
        register const uint8_t* pSrc = &font8x8_latin1[glyph][0];

        Console_DrawGlyphBitmap_Locked(self, &font8x8_latin1[glyph][0], xc, yc, fgColor, bgColor);
    }
}

// Copies the content of 'srcRect' to 'dstLoc'. Does not change the cursor
// position.
void Console_CopyRect_Locked(ConsoleRef _Nonnull self, Rect srcRect, Point dstLoc)
{
    Rect src_r = Rect_Intersection(srcRect, self->bounds);
    Rect dst_r = Rect_Intersection(Rect_Make(dstLoc.x, dstLoc.y, dstLoc.x + Rect_GetWidth(srcRect), dstLoc.y + Rect_GetHeight(srcRect)), self->bounds);
    int xOffset = dst_r.left - src_r.left;
    int yOffset = dst_r.top - src_r.top;

    if (Rect_GetWidth(src_r) == 0 || Rect_GetHeight(src_r) == 0 || Rect_GetWidth(dst_r) == 0 || (xOffset == 0 && yOffset == 0)) {
        return;
    }

    src_r.left <<= 3;
    src_r.right <<= 3;
    src_r.top <<= 3;
    src_r.bottom <<= 3;

    dst_r.left <<= 3;
    dst_r.right <<= 3;
    dst_r.top <<= 3;
    dst_r.bottom <<= 3;

    xOffset <<= 3;
    yOffset <<= 3;

    for(size_t p = 0; p < self->pixels.planeCount; p++) {
        for (int dst_y = dst_r.top; dst_y < dst_r.bottom; dst_y++) {
            const int src_y = dst_y - yOffset;

            if (src_y < src_r.top || src_y >= src_r.bottom) {
                continue;
            }

            const int src_lx = __max(dst_r.left - xOffset, src_r.left);
            const int src_rx = __min(dst_r.right - xOffset, src_r.right);
            const int src_w = (src_rx - src_lx) >> 3;
            const int dst_w = (dst_r.right - dst_r.left) >> 3;
            const size_t w = __min(src_w, dst_w);
            const size_t rowbytes = self->pixels.bytesPerRow[p];
            const uint8_t* sp = (const uint8_t*)self->pixels.plane[p] + src_y * rowbytes + (src_lx >> 3);
            uint8_t* dp = (uint8_t*)self->pixels.plane[p] + dst_y * rowbytes + (dst_r.left >> 3);

            memmove(dp, sp, w);
        }
    }
}

// Fills the content of 'rect' with the character 'ch'. Does not change the
// cursor position.
void Console_FillRect_Locked(ConsoleRef _Nonnull self, Rect rect, char ch)
{
    if (ch < 32 || ch == 127) {
        return;
    }


    const Color* bgColor = (self->characterRendition.isReverse) ? &self->foregroundColor : &self->backgroundColor;
    Rect r = Rect_Intersection(rect, self->bounds);

    if (ch == ' ') {
        r.left <<= 3;
        r.right <<= 3;
        r.top <<= 3;
        r.bottom <<= 3;

        for (size_t p = 0; p < self->pixels.planeCount; p++) {
            const int bgOne = (bgColor->u.index & (1 << p)) ? 0xff : 0;
            const size_t rowbytes = self->pixels.bytesPerRow[p];
            const size_t w = (r.right - r.left) >> 3;
            uint8_t* lp = (uint8_t*)self->pixels.plane[p] + r.top * rowbytes + (r.left >> 3);

            for (int y = r.top; y < r.bottom; y++) {
                memset(lp, bgOne, w);
                lp += rowbytes;
            }
        }

        return;
    }

    const Color* fgColor = (self->characterRendition.isReverse) ? &self->backgroundColor : &self->foregroundColor;
    const uint8_t* pGlyphBitmap = &font8x8_latin1[ch][0];

    for (int y = r.top; y < r.bottom; y++) {
        for (int x = r.left; x < r.right; x++) {
            Console_DrawGlyphBitmap_Locked(self, pGlyphBitmap, x, y, fgColor, bgColor);
        }
    }
}
