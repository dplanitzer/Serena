//
//  Console_Video.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"
#include <kern/string.h>
#include <kern/timespec.h>
#include <machine/amiga/chipset.h>
#include <machine/amiga/graphics/GraphicsDriver.h>


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
    VideoConfiguration vidCfg;
    if (chipset_is_ntsc()) {
        vidCfg.width = 640;
        vidCfg.height = 200;
        vidCfg.fps = 60;
        
        //vidCfg.width = 640;
        //vidCfg.height = 400;
        //vidCfg.fps = 30;
    } else {
        vidCfg.width = 640;
        vidCfg.height = 256;
        vidCfg.fps = 50;

        //vidCfg.width = 640;
        //vidCfg.height = 512;
        //vidCfg.fps = 25;
    }
    const int pixelsWidth = VideoConfiguration_GetPixelWidth(&vidCfg);
    const int pixelsHeight = VideoConfiguration_GetPixelHeight(&vidCfg);

    try(IOChannel_Ioctl(self->fbChannel, kFBCommand_CreateSurface, pixelsWidth, pixelsHeight, kPixelFormat_RGB_Indexed3, &self->surfaceId));
    try(IOChannel_Ioctl(self->fbChannel, kFBCommand_CreateCLUT, 32, &self->clutId));


    // Install an ANSI color table
    IOChannel_Ioctl(self->fbChannel, kFBCommand_SetCLUTEntries, self->clutId, 0, sizeof(gANSIColors), gANSIColors);


    // Make our screen the current screen
    int sc[7];
    sc[0] = SCREEN_CONFIG_FRAMEBUFFER;
    sc[1] = self->surfaceId;
    sc[2] = SCREEN_CONFIG_CLUT;
    sc[3] = self->clutId;
    sc[4] = SCREEN_CONFIG_FPS;
    sc[5] = vidCfg.fps;
    sc[6] = SCREEN_CONFIG_END;
    try(IOChannel_Ioctl(self->fbChannel, kFBCommand_SetScreenConfig, &sc[0]));


    // Get the framebuffer size
    self->pixelsWidth = pixelsWidth;
    self->pixelsHeight = pixelsHeight;


    // Allocate the text cursor (sprite)
    const bool isLace = VideoConfiguration_IsInterlaced(&vidCfg) ? true : false;
    const uint16_t* textCursorPlanes[2];
    textCursorPlanes[0] = (isLace) ? &gBlock4x4_Plane0[0] : &gBlock4x8_Plane0[0];
    textCursorPlanes[1] = (isLace) ? &gBlock4x4_Plane0[1] : &gBlock4x8_Plane0[1];
    const int textCursorWidth = (isLace) ? gBlock4x4_Width : gBlock4x8_Width;
    const int textCursorHeight = (isLace) ? gBlock4x4_Height : gBlock4x8_Height;
    try(IOChannel_Ioctl(self->fbChannel, kFBCommand_AcquireSprite, textCursorWidth, textCursorHeight, kPixelFormat_RGB_Indexed2, 0, &self->textCursor));
    try(IOChannel_Ioctl(self->fbChannel, kFBCommand_SetSpritePixels, self->textCursor, textCursorPlanes));
    self->flags.isTextCursorVisible = false;


    // Allocate the text cursor blinking timer
    timespec_from_ms(&self->cursorBlinkInterval, 500);
    self->flags.isTextCursorBlinkerEnabled = false;
    self->flags.isTextCursorOn = false;
    self->flags.isTextCursorSingleCycleOn = false;

    try(IOChannel_Ioctl(self->fbChannel, kFBCommand_UpdateDisplay));
    try(IOChannel_Ioctl(self->fbChannel, kFBCommand_MapSurface, self->surfaceId, kMapPixels_ReadWrite, &self->pixels));

catch:
    return err;
}

// Deinitializes the video output subsystem
void Console_DeinitVideo(ConsoleRef _Nonnull self)
{
    IOChannel_Ioctl(self->fbChannel, kFBCommand_UnmapSurface, self->surfaceId);

    IOChannel_Ioctl(self->fbChannel, kFBCommand_SetScreenConfig, NULL);

    IOChannel_Ioctl(self->fbChannel, kFBCommand_RelinquishSprite, self->textCursor);
    IOChannel_Ioctl(self->fbChannel, kFBCommand_DestroyCLUT, self->clutId);
    IOChannel_Ioctl(self->fbChannel, kFBCommand_DestroySurface, self->surfaceId);
    
    DispatchQueue_RemoveByTag(self->dispatchQueue, CURSOR_BLINKER_TAG);
}


// Sets the console's foreground color to the given color
void Console_SetForegroundColor_Locked(ConsoleRef _Nonnull self, Color color)
{
    assert(color.tag == kColorType_Index);
    self->foregroundColor = color;

    // Sync up the sprite color registers with the selected foreground color
    RGBColor32 clr[3];
    clr[0] = gANSIColors[color.u.index];
    clr[1] = clr[0];
    clr[2] = clr[0];
    IOChannel_Ioctl(self->fbChannel, kFBCommand_SetCLUTEntries, self->clutId, 17, 3, clr);
    IOChannel_Ioctl(self->fbChannel, kFBCommand_UpdateDisplay);
}

// Sets the console's background color to the given color
void Console_SetBackgroundColor_Locked(ConsoleRef _Nonnull self, Color color)
{
    assert(color.tag == kColorType_Index);
    self->backgroundColor = color;
}


void Console_OnTextCursorBlink(ConsoleRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    
    self->flags.isTextCursorOn = !self->flags.isTextCursorOn;
    if (self->flags.isTextCursorVisible) {
        IOChannel_Ioctl(self->fbChannel, kFBCommand_SetSpriteVisible, self->textCursor, self->flags.isTextCursorOn || self->flags.isTextCursorSingleCycleOn);
    }
    self->flags.isTextCursorSingleCycleOn = false;

    mtx_unlock(&self->mtx);
}

static void Console_UpdateCursorVisibilityAndRestartBlinking_Locked(ConsoleRef _Nonnull self)
{
    if (self->flags.isTextCursorVisible) {
        // Changing the visibility to on should restart the blinking timer if
        // blinking is on too so that we always start out with a cursor-on phase
        DispatchQueue_RemoveByTag(self->dispatchQueue, CURSOR_BLINKER_TAG);
        IOChannel_Ioctl(self->fbChannel, kFBCommand_SetSpriteVisible, self->textCursor, true);
        self->flags.isTextCursorOn = false;
        self->flags.isTextCursorSingleCycleOn = false;

        if (self->flags.isTextCursorBlinkerEnabled) {
            try_bang(DispatchQueue_DispatchAsyncPeriodically(self->dispatchQueue, 
                &TIMESPEC_ZERO,
                &self->cursorBlinkInterval,
                (VoidFunc_1)Console_OnTextCursorBlink,
                self,
                CURSOR_BLINKER_TAG));
        }
    } else {
        // Make sure that the text cursor and blinker are off
        DispatchQueue_RemoveByTag(self->dispatchQueue, CURSOR_BLINKER_TAG);
        IOChannel_Ioctl(self->fbChannel, kFBCommand_SetSpriteVisible, self->textCursor, false);
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
    IOChannel_Ioctl(self->fbChannel, kFBCommand_SetSpritePosition, self->textCursor, self->x * self->characterWidth, self->y * self->lineHeight);
    // Temporarily force the cursor to be visible, but without changing the text
    // cursor visibility state officially. We just want to make sure that the
    // cursor is on when the user types a character. This however should not
    // change anything about the blinking phase and frequency.
    if (!self->flags.isTextCursorSingleCycleOn && !self->flags.isTextCursorOn && self->flags.isTextCursorBlinkerEnabled && self->flags.isTextCursorVisible) {
        self->flags.isTextCursorSingleCycleOn = true;
        IOChannel_Ioctl(self->fbChannel, kFBCommand_SetSpriteVisible, self->textCursor, true);
    }
}


void Console_BeginDrawing_Locked(ConsoleRef _Nonnull self)
{
//    HIDManager_ShieldMouseCursor(gHIDManager, 0, 0, INT_MAX, INT_MAX);
}

void Console_EndDrawing_Locked(ConsoleRef _Nonnull self)
{
//    HIDManager_UnshieldMouseCursor(gHIDManager);
}

static void Console_DrawGlyph_Locked(ConsoleRef _Nonnull self, const Font* _Nonnull font, char ch, int xc, int yc, const Color* _Nonnull fgColor, const Color* _Nonnull bgColor)
{
    const char* gp = Font_GetGlyph(font, ch);

    for (size_t p = 0; p < self->pixels.planeCount; p++) {
        const int_fast8_t fgOne = fgColor->u.index & (1 << p);
        const int_fast8_t bgOne = bgColor->u.index & (1 << p);
        register const size_t bytesPerRow = self->pixels.bytesPerRow[p];
        register const char* sp = gp;
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

void Console_DrawChar_Locked(ConsoleRef _Nonnull self, char ch, int xc, int yc)
{
    const int maxX = self->pixelsWidth >> 3;
    const int maxY = self->pixelsHeight >> 3;
    
    if (xc >= 0 && yc >= 0 && xc < maxX && yc < maxY) {
        if (self->characterRendition.isHidden) {
            ch = ' ';
        }

        const Color* fgColor = (self->characterRendition.isReverse) ? &self->backgroundColor : &self->foregroundColor;
        const Color* bgColor = (self->characterRendition.isReverse) ? &self->foregroundColor : &self->backgroundColor;
        const Font* font = self->g[self->gl];

        Console_DrawGlyph_Locked(self, font, ch, xc, yc, fgColor, bgColor);
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
    const Font* font = self->g[self->gl];

    for (int y = r.top; y < r.bottom; y++) {
        for (int x = r.left; x < r.right; x++) {
            Console_DrawGlyph_Locked(self, font, ch, x, y, fgColor, bgColor);
        }
    }
}
