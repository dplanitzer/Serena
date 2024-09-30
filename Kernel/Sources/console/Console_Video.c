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

static const ColorTable gANSIColorTable = {
    8,
    gANSIColors
};

static const char* kCursorBlinkerTimerTag = "tag.console.cursor";


// Initializes the video subsystem
errno_t Console_InitVideoOutput(ConsoleRef _Nonnull self)
{
    decl_try_err();

    // Install an ANSI color table
    GraphicsDriver_SetCLUT(self->gdevice, &gANSIColorTable);


    // Get the framebuffer size
    const Size fbSize = GraphicsDriver_GetFramebufferSize(self->gdevice);
    self->gc.pixelsWidth = fbSize.width;
    self->gc.pixelsHeight = fbSize.height;


    // Allocate the text cursor (sprite)
    const bool isInterlaced = ScreenConfiguration_IsInterlaced(GraphicsDriver_GetCurrentScreenConfiguration(self->gdevice));
    const uint16_t* textCursorPlanes[2];
    textCursorPlanes[0] = (isInterlaced) ? &gBlock4x4_Plane0[0] : &gBlock4x8_Plane0[0];
    textCursorPlanes[1] = (isInterlaced) ? &gBlock4x4_Plane0[1] : &gBlock4x8_Plane0[1];
    const int textCursorWidth = (isInterlaced) ? gBlock4x4_Width : gBlock4x8_Width;
    const int textCursorHeight = (isInterlaced) ? gBlock4x4_Height : gBlock4x8_Height;
    try(GraphicsDriver_AcquireSprite(self->gdevice, textCursorPlanes, 0, 0, textCursorWidth, textCursorHeight, 0, &self->textCursor));
    self->flags.isTextCursorVisible = false;


    // Allocate the text cursor blinking timer
    self->flags.isTextCursorBlinkerEnabled = false;
    self->flags.isTextCursorOn = false;
    self->flags.isTextCursorSingleCycleOn = false;

catch:
    return err;
}

// Deinitializes the video output subsystem
void Console_DeinitVideoOutput(ConsoleRef _Nonnull self)
{
    GraphicsDriver_RelinquishSprite(self->gdevice, self->textCursor);
    DispatchQueue_RemoveByTag(gMainDispatchQueue, (uintptr_t)kCursorBlinkerTimerTag);
}


// Sets the console's foreground color to the given color
void Console_SetForegroundColor_Locked(ConsoleRef _Nonnull self, Color color)
{
    assert(color.tag == kColorType_Index);
    self->foregroundColor = color;

    // Sync up the sprite color registers with the selected foreground color
    GraphicsDriver_SetCLUTEntry(self->gdevice, 17, gANSIColors[color.u.index]);
    GraphicsDriver_SetCLUTEntry(self->gdevice, 18, gANSIColors[color.u.index]);
    GraphicsDriver_SetCLUTEntry(self->gdevice, 19, gANSIColors[color.u.index]);
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
        GraphicsDriver_SetSpriteVisible(self->gdevice, self->textCursor, self->flags.isTextCursorOn || self->flags.isTextCursorSingleCycleOn);
    }
    self->flags.isTextCursorSingleCycleOn = false;

    Lock_Unlock(&self->lock);
}

static void Console_UpdateCursorVisibilityAndRestartBlinking_Locked(ConsoleRef _Nonnull self)
{
    if (self->flags.isTextCursorVisible) {
        // Changing the visibility to on should restart the blinking timer if
        // blinking is on too so that we always start out with a cursor-on phase
        DispatchQueue_RemoveByTag(gMainDispatchQueue, (uintptr_t)kCursorBlinkerTimerTag);
        GraphicsDriver_SetSpriteVisible(self->gdevice, self->textCursor, true);
        self->flags.isTextCursorOn = false;
        self->flags.isTextCursorSingleCycleOn = false;

        if (self->flags.isTextCursorBlinkerEnabled) {
            try_bang(DispatchQueue_DispatchAsyncPeriodically(gMainDispatchQueue, 
                kTimeInterval_Zero,
                TimeInterval_MakeMilliseconds(500),
                (Closure1Arg_Func)Console_OnTextCursorBlink,
                self,
                (uintptr_t)kCursorBlinkerTimerTag));
        }
    } else {
        // Make sure that the text cursor and blinker are off
        DispatchQueue_RemoveByTag(gMainDispatchQueue, (uintptr_t)kCursorBlinkerTimerTag);
        GraphicsDriver_SetSpriteVisible(self->gdevice, self->textCursor, false);
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
    GraphicsDriver_SetSpritePosition(self->gdevice, self->textCursor, self->x * self->characterWidth, self->y * self->lineHeight);
    // Temporarily force the cursor to be visible, but without changing the text
    // cursor visibility state officially. We just want to make sure that the
    // cursor is on when the user types a character. This however should not
    // change anything about the blinking phase and frequency.
    if (!self->flags.isTextCursorSingleCycleOn && !self->flags.isTextCursorOn && self->flags.isTextCursorBlinkerEnabled && self->flags.isTextCursorVisible) {
        self->flags.isTextCursorSingleCycleOn = true;
        GraphicsDriver_SetSpriteVisible(self->gdevice, self->textCursor, true);
    }
}


errno_t Console_BeginDrawing_Locked(ConsoleRef _Nonnull self)
{
    return GraphicsDriver_LockFramebufferPixels(self->gdevice, kSurfaceAccess_ReadWrite, (void**)self->gc.plane, self->gc.bytesPerRow, &self->gc.planeCount);
}

void Console_EndDrawing_Locked(ConsoleRef _Nonnull self)
{
    GraphicsDriver_UnlockFramebufferPixels(self->gdevice);
}

static void Console_DrawGlyphBitmap_Locked(ConsoleRef _Nonnull self, const uint8_t* _Nonnull pSrc, int xc, int yc, const Color* _Nonnull fgColor, const Color* _Nonnull bgColor)
{
    for (size_t p = 0; p < self->gc.planeCount; p++) {
        const int_fast8_t fgOne = fgColor->u.index & (1 << p);
        const int_fast8_t bgOne = bgColor->u.index & (1 << p);
        register const size_t bytesPerRow = self->gc.bytesPerRow[p];
        register const uint8_t* sp = pSrc;
        register uint8_t* dp = self->gc.plane[p] + (yc << 3) * bytesPerRow + xc;

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
    const int maxX = self->gc.pixelsWidth >> 3;
    const int maxY = self->gc.pixelsHeight >> 3;
    
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

    for(size_t p = 0; p < self->gc.planeCount; p++) {
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
            const size_t rowbytes = self->gc.bytesPerRow[p];
            const uint8_t* sp = self->gc.plane[p] + src_y * rowbytes + (src_lx >> 3);
            uint8_t* dp = self->gc.plane[p] + dst_y * rowbytes + (dst_r.left >> 3);

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

        for (size_t p = 0; p < self->gc.planeCount; p++) {
            const int bgOne = (bgColor->u.index & (1 << p)) ? 0xff : 0;
            const size_t rowbytes = self->gc.bytesPerRow[p];
            const size_t w = (r.right - r.left) >> 3;
            uint8_t* lp = self->gc.plane[p] + r.top * rowbytes + (r.left >> 3);

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
