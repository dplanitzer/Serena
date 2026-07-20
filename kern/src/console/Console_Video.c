//
//  Console_Video.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"
#include <assert.h>
#include <string.h>
#include <driver/hw/m68k-amiga/graphics/AGADriver.h>
#include <ext/math.h>
#include <ext/nanotime.h>
#include <hal/hw/m68k-amiga/chipset.h>

#define ANSI_COLOR_COUNT    8

static void Console_OnTextCursorBlink(CursorTimer* _Nonnull timer);


static const gd_rgb32_t gANSIColors[ANSI_COLOR_COUNT] = {
    0xff000000,     // Black
    0xffff0000,     // Red
    0xff00ff00,     // Green
    0xffffff00,     // Yellow
    0xff0000ff,     // Blue
    0xffff00ff,     // Magenta
    0xff00ffff,     // Cyan
    0xffffffff,     // White
};


// Initializes the video subsystem
errno_t Console_InitVideo(ConsoleRef _Nonnull self)
{
    decl_try_err();

    try(AGADriver_CreateCommandBuffer(self->drv, 128, &self->cmdbuf));


    // Get the screen buffer information
    gd_buffer_info_t binf;

    self->pixelBufferId = AGADriver_GetScreenbuffer(self->drv);
    AGADriver_GetBufferInfo(self->drv, self->pixelBufferId, &binf);
    self->pixelsWidth = binf.width;
    self->pixelsHeight = binf.height;


    // Clear the framebuffer
    void* ip = self->cmdbuf.addr;
    ip = gdCmdClearPixels(ip, self->pixelBufferId);
    ip = gdCmdEnd(ip);

    try(AGADriver_BufferCommands(self->drv, self->pixelBufferId, self->cmdbuf.id, 0));


    // Map the console framebuffer
    try(AGADriver_MapBuffer(self->drv, self->pixelBufferId, GD_MAP_RW, &self->pixels));


    // Allocate the text cursor (sprite)
    self->textCursorSpriteId = 2;
    self->flags.isTextCursorVisible = false;
    const bool isLace = (self->pixelsHeight > MAX_PAL_HEIGHT) ? true : false;
    const uint16_t* textCursorPlanes[2];
    textCursorPlanes[0] = (isLace) ? &gBlock4x4_Plane0[0] : &gBlock4x8_Plane0[0];
    textCursorPlanes[1] = (isLace) ? &gBlock4x4_Plane0[1] : &gBlock4x8_Plane0[1];
    const int textCursorWidth = (isLace) ? gBlock4x4_Width : gBlock4x8_Width;
    const int textCursorHeight = (isLace) ? gBlock4x4_Height : gBlock4x8_Height;
    try(AGADriver_CreateBuffer(self->drv, textCursorWidth, textCursorHeight, GD_RGB_SPRITE_2, &self->textCursorBufferId));

    ip = self->cmdbuf.addr;
    ip = gdCmdDrawPixels(ip, self->textCursorBufferId, (void**)textCursorPlanes, 2, GD_COLOR_INDEX2);
    ip = gdCmdEnd(ip);

    try(AGADriver_BufferCommands(self->drv, self->textCursorBufferId, self->cmdbuf.id, 0));

    ip = self->cmdbuf.addr;
    ip = gdCmdSpriteVisible(ip, self->textCursorSpriteId, 0);
    ip = gdCmdBindSpriteBuffer(ip, GD_SPRITE_0 + self->textCursorSpriteId, self->textCursorBufferId);
    ip = gdCmdEnd(ip);

    try(AGADriver_DisplayCommands(self->drv, self->cmdbuf.id, 0));


    // Initialize the text cursor timer
    self->textCursorTimer.item = KDISPATCH_ITEM_INIT((kdispatch_item_func_t)Console_OnTextCursorBlink, NULL);
    self->textCursorTimer.console = self;
    nanotime_from_ms(&self->textCursorTimer.blinkInterval, 500);

catch:
    return err;
}

// Deinitializes the video output subsystem
void Console_DeinitVideo(ConsoleRef _Nonnull self)
{
    kdispatch_cancel_item(self->dq, &self->textCursorTimer.item);

    AGADriver_UnmapBuffer(self->drv, self->pixelBufferId);
    AGADriver_DestroyCommandBuffer(self->drv, self->cmdbuf.id);
}


// Sets the console's foreground color to the given color
void Console_SetForegroundColor_Locked(ConsoleRef _Nonnull self, Color color)
{
    assert(color.tag == kColorType_Index);
    self->foregroundColor = color;

    // Sync up the sprite color registers with the selected foreground color
    gd_rgb32_t clr[8];
    clr[0] = 0xff000000;    //XXX for mouse cursor
    clr[1] = 0xffffffff;
    clr[2] = 0xff000000;
    clr[3] = 0xff000000;
    
    clr[4] = 0xff000000;
    clr[5] = gANSIColors[color.u.index];
    clr[6] = clr[5];
    clr[7] = clr[5];

    void* ip = self->cmdbuf.addr;
    ip = gdCmdClut(ip, 16, 8, clr);
    ip = gdCmdEnd(ip);

    AGADriver_DisplayCommands(self->drv, self->cmdbuf.id, 0);
}

// Sets the console's background color to the given color
void Console_SetBackgroundColor_Locked(ConsoleRef _Nonnull self, Color color)
{
    assert(color.tag == kColorType_Index);
    self->backgroundColor = color;
}


static void Console_OnTextCursorBlink(CursorTimer* _Nonnull timer)
{
    ConsoleRef self = timer->console;

    mtx_lock(&self->mtx);
    self->flags.isTextCursorOn = !self->flags.isTextCursorOn;

    void* ip = self->cmdbuf.addr;
    ip = gdCmdSpriteVisible(ip, self->textCursorSpriteId, self->flags.isTextCursorOn);
    ip = gdCmdEnd(ip);

    AGADriver_DisplayCommands(self->drv, self->cmdbuf.id, 0);
    mtx_unlock(&self->mtx);
}


// Invoked at the end of a write():
// - show/hide text cursor based on current visibility state
// - cancel/start text cursor blink timer based on whether blinking cursor is enabled or disabled
// - make sure the text cursor is visible if it is supposed to be visible 
void Console_UpdateCursorVisuals_Locked(ConsoleRef _Nonnull self)
{
    void* ip = NULL;

    if (self->flags.isTextCursorBlinkerActive) {
        self->flags.isTextCursorBlinkerActive = false;
        kdispatch_cancel_item(self->dq, &self->textCursorTimer.item);
    }

    if (self->flags.isTextCursorVisible) {
        ip = gdCmdSpritePosition(self->cmdbuf.addr, self->textCursorSpriteId, self->x * self->characterWidth, self->y * self->lineHeight);

        if (!self->flags.isTextCursorOn) {
            self->flags.isTextCursorOn = true;
            ip = gdCmdSpriteVisible(ip, self->textCursorSpriteId, true);
        }


        if (self->flags.isTextCursorBlinkerEnabled) {
            self->flags.isTextCursorBlinkerActive = true;
            kdispatch_item_repeating(self->dq, 0, &self->textCursorTimer.blinkInterval, &self->textCursorTimer.blinkInterval, &self->textCursorTimer.item);
        }
    }
    else if (self->flags.isTextCursorOn) {
        self->flags.isTextCursorOn = false;
        ip = gdCmdSpriteVisible(self->cmdbuf.addr, self->textCursorSpriteId, false);
    }

    if (ip) {
        gdCmdEnd(ip);
        AGADriver_DisplayCommands(self->drv, self->cmdbuf.id, 0);
    }
}

static void Console_DrawGlyph_Locked(ConsoleRef _Nonnull self, const Font* _Nonnull font, char ch, int xc, int yc, const Color* _Nonnull fgColor, const Color* _Nonnull bgColor)
{
    const char* gp = Font_GetGlyph(font, ch);

    for (size_t p = 0; p < self->pixels.planeCount; p++) {
        const int_fast8_t fgOne = fgColor->u.index & (1 << p);
        const int_fast8_t bgOne = bgColor->u.index & (1 << p);
        register const size_t bytesPerRow = self->pixels.bytesPerRow;
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
    const size_t rowbytes = self->pixels.bytesPerRow;

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
        const size_t rowbytes = self->pixels.bytesPerRow;

        r.left <<= 3;
        r.right <<= 3;
        r.top <<= 3;
        r.bottom <<= 3;

        for (size_t p = 0; p < self->pixels.planeCount; p++) {
            const int bgOne = (bgColor->u.index & (1 << p)) ? 0xff : 0;
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
