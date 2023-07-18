//
//  Console.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Console.h"
#include "Heap.h"
#include "SystemGlobals.h"


//
// Fonts
//
extern const Byte font8x8_latin1[128][8];
extern const Byte font8x8_dingbat[160][8];
#define GLYPH_WIDTH     8
#define GLYPH_HEIGHT    8


// Returns the console for the main screen.
Console* _Nonnull Console_GetMain(void)
{
    return SystemGlobals_Get()->console;
}


// Creates a new console object. This console will display its output on the
// provided graphics device.
// \param pGDevice the graphics device
// \return the console; NULL on failure
ErrorCode Console_Create(GraphicsDriverRef _Nonnull pGDevice, Console* _Nullable * _Nonnull pOutConsole)
{
    decl_try_err();
    Console* pConsole;
    const Surface* pFramebuffer;

    try_null(pFramebuffer, GraphicsDriver_GetFramebuffer(pGDevice), ENODEV);
    try(kalloc_cleared(sizeof(Console), (Byte**) &pConsole));
    
    pConsole->pGDevice = pGDevice;
    Lock_Init(&pConsole->lock);
    pConsole->x = 0;
    pConsole->y = 0;
    pConsole->cols = pFramebuffer->width / GLYPH_WIDTH;
    pConsole->rows = pFramebuffer->height / GLYPH_HEIGHT;
    pConsole->flags |= CONSOLE_FLAG_AUTOSCROLL_TO_BOTTOM;
    pConsole->lineBreakMode = kLineBreakMode_WrapCharacter;
    pConsole->tabWidth = 8;
    
    Console_ClearScreen(pConsole);
    
    *pOutConsole = pConsole;
    return err;
    
catch:
    Console_Destroy(pConsole);
    *pOutConsole = NULL;
    return err;
}

// Deallocates the console.
// \param pConsole the console
void Console_Destroy(Console* _Nullable pConsole)
{
    if (pConsole) {
        pConsole->pGDevice = NULL;
        Lock_Deinit(&pConsole->lock);
        kfree((Byte*)pConsole);
    }
}

// Returns the console bounds.
Rect Console_GetBounds(Console* _Nonnull pConsole)
{
    return Rect_Make(0, 0, pConsole->cols, pConsole->rows);
}

// Clears the console screen.
// \param pConsole the console
void Console_ClearScreen(Console* _Nonnull pConsole)
{
    pConsole->x = 0;
    pConsole->y = 0;
    GraphicsDriver_Clear(pConsole->pGDevice);
}

// Clears the specified line. Does not change the
// cursor position.
void Console_ClearLine(Console* _Nonnull pConsole, Int y)
{
    const Rect bounds = Rect_Make(0, 0, pConsole->cols, pConsole->rows);
    const Rect r = Rect_Intersection(Rect_Make(0, y, pConsole->cols, 1), bounds);
    
    GraphicsDriver_FillRect(pConsole->pGDevice,
                            Rect_Make(r.x * GLYPH_WIDTH, r.y * GLYPH_HEIGHT, r.width * GLYPH_WIDTH, r.height * GLYPH_HEIGHT),
                            Color_MakeIndex(0));
}

// Copies the content of 'srcRect' to 'dstLoc'. Does not change the cursor
// position.
void Console_CopyRect(Console* _Nonnull pConsole, Rect srcRect, Point dstLoc)
{
    GraphicsDriver_CopyRect(pConsole->pGDevice,
                            Rect_Make(srcRect.x * GLYPH_WIDTH, srcRect.y * GLYPH_HEIGHT, srcRect.width * GLYPH_WIDTH, srcRect.height * GLYPH_HEIGHT),
                            Point_Make(dstLoc.x * GLYPH_WIDTH, dstLoc.y * GLYPH_HEIGHT));
}

// Fills the content of 'rect' with the character 'ch'. Does not change the
// cursor position.
void Console_FillRect(Console* _Nonnull pConsole, Rect rect, Character ch)
{
    const Rect bounds = Rect_Make(0, 0, pConsole->cols, pConsole->rows);
    const Rect r = Rect_Intersection(rect, bounds);

    if (ch == ' ') {
        GraphicsDriver_FillRect(pConsole->pGDevice,
                                Rect_Make(r.x * GLYPH_WIDTH, r.y * GLYPH_HEIGHT, r.width * GLYPH_WIDTH, r.height * GLYPH_HEIGHT),
                                Color_MakeIndex(0));
    }
    else if (ch < 32 || ch == 127) {
        // Control characters -> do nothing
    }
    else {
        for (Int y = r.y; y < r.y + r.height; y++) {
            for (Int x = r.x; x < r.x + r.width; x++) {
                GraphicsDriver_BlitGlyph_8x8bw(pConsole->pGDevice, &font8x8_latin1[ch][0], x, y);
            }
        }
    }
}

// Scrolls the content of the console screen. 'clipRect' defines a viewport through
// which a virtual document is visible. This viewport is scrolled by 'dX' / 'dY'
// pixels. Positive values move the viewport down (and scroll the virtual document
// up) and negative values move the viewport up (and scroll the virtual document
// down).
void Console_ScrollBy(Console* _Nonnull pConsole, Rect clipRect, Point dXY)
{
    if (dXY.x == 0 && dXY.y == 0) {
        return;
    }
    
    const Rect bounds = Rect_Make(0, 0, pConsole->cols, pConsole->rows);
    const Int hExposedWidth = min(abs(dXY.x), clipRect.width);
    const Int vExposedHeight = min(abs(dXY.y), clipRect.height);
    Rect copyRect, hClearRect, vClearRect;
    Point dstLoc;
    
    copyRect.x = (dXY.x < 0) ? clipRect.x : min(clipRect.x + dXY.x, Rect_GetMaxX(clipRect));
    copyRect.y = (dXY.y < 0) ? clipRect.y : min(clipRect.y + dXY.y, Rect_GetMaxY(clipRect));
    copyRect.width = clipRect.width - hExposedWidth;
    copyRect.height = clipRect.height - vExposedHeight;
    
    dstLoc.x = (dXY.x < 0) ? clipRect.x - dXY.x : clipRect.x;
    dstLoc.y = (dXY.y < 0) ? clipRect.y - dXY.y : clipRect.y;
    
    hClearRect.x = clipRect.x;
    hClearRect.y = (dXY.y < 0) ? clipRect.y : Rect_GetMaxY(clipRect) - vExposedHeight;
    hClearRect.width = clipRect.width;
    hClearRect.height = vExposedHeight;
    
    vClearRect.x = (dXY.x < 0) ? clipRect.x : Rect_GetMaxX(clipRect) - hExposedWidth;
    vClearRect.y = (dXY.y < 0) ? clipRect.y : clipRect.y + vExposedHeight;
    vClearRect.width = hExposedWidth;
    vClearRect.height = clipRect.height - vExposedHeight;

    Console_CopyRect(pConsole, copyRect, dstLoc);
    Console_FillRect(pConsole, hClearRect, ' ');
    Console_FillRect(pConsole, vClearRect, ' ');
}

// Moves the console position by the given delta values.
// \param pConsole the console
// \param dx the X delta
// \param dy the Y delta
void Console_MoveCursor(Console* _Nonnull pConsole, Int dx, Int dy)
{
    Console_MoveCursorTo(pConsole, pConsole->x + dx, pConsole->y + dy);
}

// Sets the console position. The next print() will start printing at this
// location.
// \param pConsole the console
// \param x the X position
// \param y the Y position
void Console_MoveCursorTo(Console* _Nonnull pConsole, Int x, Int y)
{
    pConsole->x = x;
    pConsole->y = y;
}

// Prints the given character to the console.
// \param pConsole the console
// \param ch the character
void Console_DrawCharacter(Console* _Nonnull pConsole, Character ch)
{
    const Bool isAutoscrollEnabled = (pConsole->flags & CONSOLE_FLAG_AUTOSCROLL_TO_BOTTOM) != 0;
    
    switch (ch) {
        case '\0':
            break;
            
        case '\t':
            if (pConsole->tabWidth > 0) {
                // go to the next tab stop
                pConsole->x = (pConsole->x / pConsole->tabWidth + 1) * pConsole->tabWidth;
            
                if (pConsole->lineBreakMode == kLineBreakMode_WrapCharacter && pConsole->x >= pConsole->cols && pConsole->cols > 0) {
                    // Wrap-by-character is enabled. Treat this like a newline aka move to the first tab stop in the next line
                    Console_DrawCharacter(pConsole, '\n');
                }
            }
            break;

        case '\n':
            pConsole->x = 0;
            // fall through
            
        case 11:    // Vertical tab (always 1)
            pConsole->y++;
            if (pConsole->y == pConsole->rows && isAutoscrollEnabled) {
                Console_ScrollBy(pConsole, Console_GetBounds(pConsole), Point_Make(0, 1));
                pConsole->y--;
            }
            break;
            
        case '\r':
            pConsole->x = 0;
            break;
            
        case 8:     // BS Backspace
            if (pConsole->x > 0) {
                // BS moves 1 cell to the left
                Console_CopyRect(pConsole, Rect_Make(pConsole->x, pConsole->y, pConsole->cols - pConsole->x, 1), Point_Make(pConsole->x - 1, pConsole->y));
                Console_FillRect(pConsole, Rect_Make(pConsole->cols - 1, pConsole->y, 1, 1), ' ');
                pConsole->x--;
            }
            break;
            
        case 12:    // FF Form feed (new page / clear screen)
            Console_ClearScreen(pConsole);
            break;
            
        case 127:   // DEL Delete
            if (pConsole->x < pConsole->cols - 1) {
                // DEL does not change the position.
                Console_CopyRect(pConsole, Rect_Make(pConsole->x + 1, pConsole->y, pConsole->cols - (pConsole->x + 1), 1), Point_Make(pConsole->x, pConsole->y));
                Console_FillRect(pConsole, Rect_Make(pConsole->cols - 1, pConsole->y, 1, 1), ' ');
            }
            break;
            
        case 141:   // RI Reverse line feed
            pConsole->y--;
            break;
            
        case 148:   // CCH Cancel character (replace the previous character with a space)
            if (pConsole->x > 0) {
                pConsole->x--;
                GraphicsDriver_BlitGlyph_8x8bw(pConsole->pGDevice, &font8x8_latin1[0x20][0], pConsole->x, pConsole->y);
            }
            break;
            
        default:
            if (ch < 32) {
                // non-prinable (control code 0) characters do nothing
                break;
            }
            
            if (pConsole->lineBreakMode == kLineBreakMode_WrapCharacter && pConsole->x >= pConsole->cols && pConsole->cols > 0) {
                // wrap the line of wrap-by-character is active
                pConsole->x = 0;
                pConsole->y++;
            }

            if (pConsole->y >= pConsole->rows && isAutoscrollEnabled && pConsole->x >= 0 && pConsole->x < pConsole->cols && pConsole->rows > 0) {
                // do a scroll-to-bottom if auto-scrolling is active and the character we're about to print is
                // in the visible portion of the console
                const Int yDelta = pConsole->y - pConsole->rows - 1;
                Console_ScrollBy(pConsole, Console_GetBounds(pConsole), Point_Make(0, yDelta));
                pConsole->y -= yDelta;
            }
            
            if ((pConsole->x >= 0 && pConsole->x < pConsole->cols) && (pConsole->y >= 0 && pConsole->y < pConsole->rows)) {
                GraphicsDriver_BlitGlyph_8x8bw(pConsole->pGDevice, &font8x8_latin1[ch][0], pConsole->x, pConsole->y);
            }
            pConsole->x++;
            break;
    }
}

// Prints the given nul-terminated string to the console.
// \param pConsole the console
// \param str the nul-terminated string
void Console_DrawString(Console* _Nonnull pConsole, const Character* _Nonnull str)
{
    register char ch;
    
    while ((ch = *str++) != '\0') {
        Console_DrawCharacter(pConsole, ch);
    }
}
