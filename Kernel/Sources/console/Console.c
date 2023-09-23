//
//  Console.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"

static ErrorCode Console_ResetState_Locked(ConsoleRef _Nonnull pConsole);
static void Console_ClearScreen_Locked(Console* _Nonnull pConsole);
static void Console_MoveCursorTo_Locked(Console* _Nonnull pConsole, Int x, Int y);
static void Console_Execute_LF_Locked(ConsoleRef _Nonnull pConsole);
static void Console_ParseInputBytes_Locked(struct vtparse* pParse, vtparse_action_t action, unsigned char b);


// Creates a new console object. This console will display its output on the
// provided graphics device.
// \param pEventDriver the event driver to provide keyboard input
// \param pGDevice the graphics device
// \return the console; NULL on failure
ErrorCode Console_Create(EventDriverRef _Nonnull pEventDriver, GraphicsDriverRef _Nonnull pGDevice, ConsoleRef _Nullable * _Nonnull pOutConsole)
{
    decl_try_err();
    Console* pConsole;

    try(kalloc_cleared(sizeof(Console), (Byte**) &pConsole));
    
    Lock_Init(&pConsole->lock);

    pConsole->pEventDriver = pEventDriver;
    pConsole->pGDevice = pGDevice;

    vtparse_init(&pConsole->vtparse, Console_ParseInputBytes_Locked, pConsole);

    pConsole->keyMapper.map = (const KeyMap*) &gKeyMap_usa[0];
    pConsole->keyMapper.capacity = KeyMap_GetMaxOutputByteCount(pConsole->keyMapper.map);
    try(kalloc_cleared(pConsole->keyMapper.capacity, &pConsole->keyMapper.buffer));
    
    try(Console_ResetState_Locked(pConsole));
    Console_ClearScreen_Locked(pConsole);
    
    *pOutConsole = pConsole;
    return err;
    
catch:
    Console_Destroy(pConsole);
    *pOutConsole = NULL;
    return err;
}

// Deallocates the console.
// \param pConsole the console
void Console_Destroy(ConsoleRef _Nullable pConsole)
{
    if (pConsole) {
        pConsole->pGDevice = NULL;
        pConsole->pEventDriver = NULL;
        kfree(pConsole->keyMapper.buffer);
        pConsole->keyMapper.buffer = NULL;
        TabStops_Deinit(&pConsole->hTabStops);
        TabStops_Deinit(&pConsole->vTabStops);
        Lock_Deinit(&pConsole->lock);
        kfree((Byte*)pConsole);
    }
}

static ErrorCode Console_ResetState_Locked(ConsoleRef _Nonnull pConsole)
{
    static const RGBColor bgColor = {0x00, 0x00, 0x00};
    static const RGBColor fgColor = {0x00, 0xff, 0x00};
    decl_try_err();
    const Surface* pFramebuffer;
    
    try_null(pFramebuffer, GraphicsDriver_GetFramebuffer(pConsole->pGDevice), ENODEV);
    pConsole->bounds = Rect_Make(0, 0, pFramebuffer->width / GLYPH_WIDTH, pFramebuffer->height / GLYPH_HEIGHT);
    pConsole->savedCursorPosition = Point_Zero;

    GraphicsDriver_SetCLUTEntry(pConsole->pGDevice, 0, &bgColor);
    GraphicsDriver_SetCLUTEntry(pConsole->pGDevice, 1, &fgColor);

    TabStops_Deinit(&pConsole->hTabStops);
    try(TabStops_Init(&pConsole->hTabStops, __max(pConsole->bounds.width / 8, 0), 8));

    TabStops_Deinit(&pConsole->vTabStops);
    try(TabStops_Init(&pConsole->vTabStops, 0, 0));

    Console_MoveCursorTo_Locked(pConsole, 0, 0);
    pConsole->lineBreakMode = kLineBreakMode_WrapCharacterAndScroll;

    return EOK;

catch:
    return err;
}

// Clears the console screen.
// \param pConsole the console
static void Console_ClearScreen_Locked(ConsoleRef _Nonnull pConsole)
{
    GraphicsDriver_Clear(pConsole->pGDevice);
}

// Clears the specified line. Does not change the cursor position.
static void Console_ClearLine_Locked(ConsoleRef _Nonnull pConsole, Int y, ClearLineMode mode)
{
    if (Rect_Contains(pConsole->bounds, 0, y)) {
        Int x, w;

        switch (mode) {
            case kClearLineMode_Whole:
                x = 0;
                w = pConsole->bounds.width;
                break;

            case kClearLineMode_ToBeginning:
                x = 0;
                w = pConsole->x;
                break;

            case kClearLineMode_ToEnd:
                x = pConsole->x;
                w = pConsole->bounds.width - x;
                break;

            default:
                abort();
        }

        GraphicsDriver_FillRect(pConsole->pGDevice,
                            Rect_Make(x * GLYPH_WIDTH, y * GLYPH_HEIGHT, w * GLYPH_WIDTH, GLYPH_HEIGHT),
                            Color_MakeIndex(0));
    }
}

// Copies the content of 'srcRect' to 'dstLoc'. Does not change the cursor
// position.
static void Console_CopyRect_Locked(ConsoleRef _Nonnull pConsole, Rect srcRect, Point dstLoc)
{
    GraphicsDriver_CopyRect(pConsole->pGDevice,
                            Rect_Make(srcRect.x * GLYPH_WIDTH, srcRect.y * GLYPH_HEIGHT, srcRect.width * GLYPH_WIDTH, srcRect.height * GLYPH_HEIGHT),
                            Point_Make(dstLoc.x * GLYPH_WIDTH, dstLoc.y * GLYPH_HEIGHT));
}

// Fills the content of 'rect' with the character 'ch'. Does not change the
// cursor position.
static void Console_FillRect_Locked(ConsoleRef _Nonnull pConsole, Rect rect, Character ch)
{
    const Rect r = Rect_Intersection(rect, pConsole->bounds);

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
static void Console_ScrollBy_Locked(ConsoleRef _Nonnull pConsole, Int dX, Int dY)
{
    if (dX == 0 && dY == 0) {
        return;
    }
    
    const Rect clipRect = pConsole->bounds;
    const Int hExposedWidth = __min(__abs(dX), clipRect.width);
    const Int vExposedHeight = __min(__abs(dY), clipRect.height);
    Rect copyRect, hClearRect, vClearRect;
    Point dstLoc;
    
    copyRect.x = (dX < 0) ? clipRect.x : __min(clipRect.x + dX, Rect_GetMaxX(clipRect));
    copyRect.y = (dY < 0) ? clipRect.y : __min(clipRect.y + dY, Rect_GetMaxY(clipRect));
    copyRect.width = clipRect.width - hExposedWidth;
    copyRect.height = clipRect.height - vExposedHeight;
    
    dstLoc.x = (dX < 0) ? clipRect.x - dX : clipRect.x;
    dstLoc.y = (dY < 0) ? clipRect.y - dY : clipRect.y;
    
    hClearRect.x = clipRect.x;
    hClearRect.y = (dY < 0) ? clipRect.y : Rect_GetMaxY(clipRect) - vExposedHeight;
    hClearRect.width = clipRect.width;
    hClearRect.height = vExposedHeight;
    
    vClearRect.x = (dX < 0) ? clipRect.x : Rect_GetMaxX(clipRect) - hExposedWidth;
    vClearRect.y = (dY < 0) ? clipRect.y : clipRect.y + vExposedHeight;
    vClearRect.width = hExposedWidth;
    vClearRect.height = clipRect.height - vExposedHeight;

    Console_CopyRect_Locked(pConsole, copyRect, dstLoc);
    Console_FillRect_Locked(pConsole, hClearRect, ' ');
    Console_FillRect_Locked(pConsole, vClearRect, ' ');
}

static void Console_DeleteLines_Locked(ConsoleRef _Nonnull pConsole, Int nLines)
{
    const Int copyFromLine = pConsole->y + nLines;
    const Int linesToCopy = __max(pConsole->bounds.height - 1 - copyFromLine, 0);
    const Int clearFromLine = pConsole->y + linesToCopy;
    const Int linesToClear = __max(pConsole->bounds.height - 1 - clearFromLine, 0);

    if (linesToCopy > 0) {
        Console_CopyRect_Locked(pConsole,
            Rect_Make(0, copyFromLine, pConsole->bounds.width, linesToCopy),
            Point_Make(0, pConsole->y));
    }

    if (linesToClear > 0) {
        Console_FillRect_Locked(pConsole,
            Rect_Make(0, clearFromLine, pConsole->bounds.width, linesToClear),
            ' ');
    }
}

// Sets the console position. The next print() will start printing at this
// location.
// \param pConsole the console
// \param x the X position
// \param y the Y position
static void Console_MoveCursorTo_Locked(Console* _Nonnull pConsole, Int x, Int y)
{
    pConsole->x = __max(__min(x, pConsole->bounds.width - 1), 0);
    pConsole->y = __max(__min(y, pConsole->bounds.height - 1), 0);
}

// Moves the console position by the given delta values.
// \param pConsole the console
// \param dx the X delta
// \param dy the Y delta
static void Console_MoveCursor_Locked(ConsoleRef _Nonnull pConsole, Int dx, Int dy)
{
    const Int eX = pConsole->bounds.width - 1;
    const Int eY = pConsole->bounds.height - 1;
    Int x = pConsole->x + dx;
    Int y = pConsole->y + dy;

    switch (pConsole->lineBreakMode) {
        case kLineBreakMode_Clamp:
            x = __max(__min(x, eX), 0);
            y = __max(__min(y, eY), 0);
            break;

        case kLineBreakMode_WrapCharacter:
            if (x < 0) {
                x = eX;
                y--;
            }
            else if (x >= eX) {
                x = 0;
                y++;
            }

            if (y < 0) {
                y = 0;
            }
            else if (y >= eY) {
                y = eY;
            }
            break;

        case kLineBreakMode_WrapCharacterAndScroll:
            if (x < 0) {
                x = eX;
                y--;
            }
            else if (x >= eX) {
                x = 0;
                y++;
            }

            if (y < 0) {
                Console_ScrollBy_Locked(pConsole, 0, y);
                y = 0;
            }
            else if (y >= eY) {
                Console_ScrollBy_Locked(pConsole, 0, y - eY);
                y = eY;
            }
            break;

        default:
            abort();
            break;
    }

    pConsole->x = x;
    pConsole->y = y;
}


////////////////////////////////////////////////////////////////////////////////
// Processing input bytes
////////////////////////////////////////////////////////////////////////////////

// Interprets the given byte as a character, maps it to a glyph and prints it.
// \param pConsole the console
// \param ch the character
static void Console_PrintByte_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    // The cursor position is always valid and inside the framebuffer
    GraphicsDriver_BlitGlyph_8x8bw(pConsole->pGDevice, &font8x8_latin1[ch][0], pConsole->x, pConsole->y);
    Console_MoveCursor_Locked(pConsole, 1, 0);
}

static void Console_Execute_BEL_Locked(ConsoleRef _Nonnull pConsole)
{
    // XXX flash screen?
}

static void Console_Execute_BS_Locked(ConsoleRef _Nonnull pConsole)
{
    if (pConsole->x > 0) {
        // BS moves 1 cell to the left
        Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x, pConsole->y, pConsole->bounds.width - pConsole->x, 1), Point_Make(pConsole->x - 1, pConsole->y));
        Console_FillRect_Locked(pConsole, Rect_Make(pConsole->bounds.width - 1, pConsole->y, 1, 1), ' ');
        Console_MoveCursor_Locked(pConsole, -1, 0);
    }
}

static void Console_Execute_HT_Locked(ConsoleRef _Nonnull pConsole)
{
    Console_MoveCursorTo_Locked(pConsole, TabStops_GetNextStop(&pConsole->hTabStops, pConsole->x, pConsole->bounds.width), pConsole->y);
}

static void Console_Execute_HTS_Locked(ConsoleRef _Nonnull pConsole)
{
    TabStops_InsertStop(&pConsole->hTabStops, pConsole->x);
}

static void Console_Execute_VT_Locked(ConsoleRef _Nonnull pConsole)
{
    if (pConsole->vTabStops.count > 0) {
        Console_MoveCursorTo_Locked(pConsole, pConsole->x, TabStops_GetNextStop(&pConsole->vTabStops, pConsole->y, pConsole->bounds.height));
    } else {
        Console_Execute_LF_Locked(pConsole);
    }
}

static void Console_Execute_VTS_Locked(ConsoleRef _Nonnull pConsole)
{
    TabStops_InsertStop(&pConsole->vTabStops, pConsole->y);
}

// Line feed may be IND or NEL depending on a setting (that doesn't exist yet)
static void Console_Execute_LF_Locked(ConsoleRef _Nonnull pConsole)
{
    Console_MoveCursor_Locked(pConsole, -pConsole->x, 1);
}

static void Console_Execute_DEL_Locked(ConsoleRef _Nonnull pConsole)
{
    if (pConsole->x < pConsole->bounds.width - 1) {
        // DEL does not change the position.
        Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x + 1, pConsole->y, pConsole->bounds.width - (pConsole->x + 1), 1), Point_Make(pConsole->x, pConsole->y));
        Console_FillRect_Locked(pConsole, Rect_Make(pConsole->bounds.width - 1, pConsole->y, 1, 1), ' ');
    }
}

static void Console_Execute_CCH_Locked(ConsoleRef _Nonnull pConsole)
{
    if (pConsole->x > 0) {
        Console_MoveCursor_Locked(pConsole, -1, 0);
        GraphicsDriver_BlitGlyph_8x8bw(pConsole->pGDevice, &font8x8_latin1[0x20][0], pConsole->x, pConsole->y);
    }
}

// Interprets the given byte as a C0/C1 control character and either executes it or ignores it.
// \param pConsole the console
// \param ch the character
static void Console_ExecuteByte_C0_C1_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    switch (ch) {
        case 0x07:  // BEL (Bell)
            Console_Execute_BEL_Locked(pConsole);
            break;

        case 0x08:  // BS (Backspace)
            Console_Execute_BS_Locked(pConsole);
            break;

        case 0x09:  // HT (Tab)
            Console_Execute_HT_Locked(pConsole);
            break;

        case 0x0a:  // LF (Line Feed)
        case 0x0c:  // FF (Form Feed / New Page / Clear Screen)
            Console_Execute_LF_Locked(pConsole);
            break;

        case 0x0b:  // VT (Vertical Tab)
            Console_Execute_VT_Locked(pConsole);
            break;

        case 0x0d:  // CR (Carriage Return)
            Console_MoveCursorTo_Locked(pConsole, 0, pConsole->y);
            break;
            
        case 0x7f:  // DEL (Delete)
            Console_Execute_DEL_Locked(pConsole);
            break;
            
        case 0x84:  // IND (Index)
            Console_MoveCursor_Locked(pConsole, 0, 1);
            break;

        case 0x85:  // NEL (Next Line)
            Console_MoveCursor_Locked(pConsole, -pConsole->x, 1);
            break;

        case 0x88:  // HTS (Horizontal Tabulation Set)
            Console_Execute_HTS_Locked(pConsole);
            break;

        case 0x8a:  // VTS (Vertical Tabulation Set)
            Console_Execute_VTS_Locked(pConsole);
            break;

        case 0x8d:  // RI (Reverse Line Feed)
            Console_MoveCursor_Locked(pConsole, 0, -1);
            break;
            
        case 0x94:  // CCH (Cancel Character (replace the previous character with a space))
            Console_Execute_CCH_Locked(pConsole);
            break;
            
        default:
            // Ignore it
            break;
    }
}

static Int get_csi_parameter(ConsoleRef _Nonnull pConsole, Int defValue)
{
    const Int val = (pConsole->vtparse.num_params > 0) ? pConsole->vtparse.params[0] : 0;
    return (val > 0) ? val : defValue;
}

static Int get_nth_csi_parameter(ConsoleRef _Nonnull pConsole, Int idx, Int defValue)
{
    const Int val = (pConsole->vtparse.num_params > idx) ? pConsole->vtparse.params[idx] : 0;
    return (val > 0) ? val : defValue;
}

static void Console_Execute_CSI_ED_Locked(ConsoleRef _Nonnull pConsole, Int mode)
{
    switch (mode) {
        case 0:
            Console_ClearLine_Locked(pConsole, pConsole->y, kClearLineMode_ToEnd);
            break;

        case 1:
            Console_ClearLine_Locked(pConsole, pConsole->y, kClearLineMode_ToBeginning);
            break;

        case 2:     // Clear screen
            // Fall through
        case 3:     // Clear screen + scrollback buffer (we got none)
            Console_ClearScreen_Locked(pConsole);
            break;

        default:
            // Ignore
            break;
    }
}

static void Console_Execute_CSI_CTC_Locked(ConsoleRef _Nonnull pConsole, Int op)
{
    switch (op) {
        case 0:
            Console_Execute_HTS_Locked(pConsole);
            break;

        case 1:
            Console_Execute_VTS_Locked(pConsole);
            break;

        case 2:
            TabStops_RemoveStop(&pConsole->hTabStops, pConsole->x);
            break;

        case 3:
            TabStops_RemoveStop(&pConsole->vTabStops, pConsole->y);
            break;

        case 4:
        case 5:
            TabStops_RemoveAllStops(&pConsole->hTabStops);
            break;

        case 6:
            TabStops_RemoveAllStops(&pConsole->vTabStops);
            break;

        default:
            // Ignore
            break;
    }
}

static void Console_Execute_CSI_TBC_Locked(ConsoleRef _Nonnull pConsole, Int op)
{
    switch (op) {
        case 0:
            TabStops_RemoveStop(&pConsole->hTabStops, pConsole->x);
            break;

        case 1:
            TabStops_RemoveStop(&pConsole->vTabStops, pConsole->y);
            break;

        case 2:
        case 3:
            TabStops_RemoveAllStops(&pConsole->hTabStops);
            break;

        case 4:
            TabStops_RemoveAllStops(&pConsole->vTabStops);
            break;

        default:
            // Ignore
            break;
    }
}

static void Console_Execute_CSI_ECH_Locked(ConsoleRef _Nonnull pConsole, Int nChars)
{
    Console_FillRect_Locked(pConsole, 
        Rect_Make(pConsole->x, pConsole->y, nChars, 1),
        ' ');
}

static void Console_CSI_Dispatch_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    switch (ch) {
        case 'A':
            Console_MoveCursor_Locked(pConsole, 0, -get_csi_parameter(pConsole, 1));
            break;

        case 'B':
            Console_MoveCursor_Locked(pConsole, 0, get_csi_parameter(pConsole, 1));
            break;

        case 'C':
            Console_MoveCursor_Locked(pConsole, get_csi_parameter(pConsole, 1), 0);
            break;

        case 'D':
            Console_MoveCursor_Locked(pConsole, -get_csi_parameter(pConsole, 1), 0);
            break;

        case 'E':
            Console_MoveCursorTo_Locked(pConsole, 0, pConsole->y + get_csi_parameter(pConsole, 1));
            break;

        case 'F':
            Console_MoveCursorTo_Locked(pConsole, 0, pConsole->y - get_csi_parameter(pConsole, 1));
            break;

        case 'G':
            Console_MoveCursorTo_Locked(pConsole, get_csi_parameter(pConsole, 1) - 1, pConsole->y);
            break;

        case 'H':
        case 'f':
            Console_MoveCursorTo_Locked(pConsole, get_nth_csi_parameter(pConsole, 1, 1) - 1, get_nth_csi_parameter(pConsole, 0, 1) - 1);
            break;

        case 'I':
            Console_MoveCursor_Locked(pConsole, TabStops_GetNextNthStop(&pConsole->hTabStops, pConsole->x, get_csi_parameter(pConsole, 1), pConsole->bounds.width), 0);
            break;

        case 'J':
            Console_Execute_CSI_ED_Locked(pConsole, get_csi_parameter(pConsole, 0));
            break;

        case 'K': {
            const Int mode = get_csi_parameter(pConsole, 0);
            if (mode < 3) {
                Console_ClearLine_Locked(pConsole, pConsole->y, (ClearLineMode) mode);
            }
            break;
        }

        case 'M':
            Console_DeleteLines_Locked(pConsole, get_csi_parameter(pConsole, 1));
            break;

        case 'P':
            Console_Execute_BS_Locked(pConsole);    // XXx handle parameter; handle display, line, etc modes
            break;

        case 'S':
            Console_ScrollBy_Locked(pConsole, 0, -get_csi_parameter(pConsole, 1));
            break;

        case 'T':
            Console_ScrollBy_Locked(pConsole, 0, get_csi_parameter(pConsole, 1));
            break;

        case 'W':
            Console_Execute_CSI_CTC_Locked(pConsole, get_csi_parameter(pConsole, 0));
            break;

        case 'X':
            Console_Execute_CSI_ECH_Locked(pConsole, get_csi_parameter(pConsole, 1));
            break;

        case 'Y':
            Console_MoveCursor_Locked(pConsole, 0, TabStops_GetNextNthStop(&pConsole->vTabStops, pConsole->y, get_csi_parameter(pConsole, 1), pConsole->bounds.height));
            break;

        case 'Z':
            Console_MoveCursor_Locked(pConsole, TabStops_GetPreviousNthStop(&pConsole->hTabStops, pConsole->x, get_csi_parameter(pConsole, 1)), 0);
            break;

        case '`':
            Console_MoveCursorTo_Locked(pConsole, get_csi_parameter(pConsole, 1) - 1, pConsole->y);
            break;

        case 'a':
            Console_MoveCursor_Locked(pConsole, get_csi_parameter(pConsole, 1), 0);
            break;

        case 'd':
            Console_MoveCursorTo_Locked(pConsole, 0, get_csi_parameter(pConsole, 1) - 1);
            break;

        case 'e':
            Console_MoveCursor_Locked(pConsole, 0, get_csi_parameter(pConsole, 1));
            break;

        case 'g':
            Console_Execute_CSI_TBC_Locked(pConsole, get_csi_parameter(pConsole, 0));
            break;

        default:
            // Ignore
            break;
    }
}

static void Console_ESC_Dispatch_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    switch (ch) {
        case 'c':
            Console_ResetState_Locked(pConsole);
            break;

        case '7':
        case 's':
            pConsole->savedCursorPosition = Point_Make(pConsole->x, pConsole->y);
            break;

        case '8':
        case 'u':
            Console_MoveCursorTo_Locked(pConsole, pConsole->savedCursorPosition.x, pConsole->savedCursorPosition.y);
            break;

        case 'D':
            Console_MoveCursor_Locked(pConsole, 0, 1);
            break;

        case 'E':
            Console_MoveCursor_Locked(pConsole, -pConsole->x, 1);
            break;

        case 'H':
            Console_Execute_HTS_Locked(pConsole);
            break;

        case 'J':
            Console_Execute_VTS_Locked(pConsole);
            break;

        case 'M':
            Console_MoveCursor_Locked(pConsole, 0, -1);
            break;
            
        default:
            // Ignore
            break;
    }
}

static void Console_ParseInputBytes_Locked(struct vtparse* pParse, vtparse_action_t action, unsigned char b)
{
    ConsoleRef pConsole = (ConsoleRef) pParse->user_data;

    switch (action) {
        case VTPARSE_ACTION_CSI_DISPATCH:
            Console_CSI_Dispatch_Locked(pConsole, b);
            break;

        case VTPARSE_ACTION_ESC_DISPATCH:
            Console_ESC_Dispatch_Locked(pConsole, b);
            break;

        case VTPARSE_ACTION_EXECUTE:
            Console_ExecuteByte_C0_C1_Locked(pConsole, b);
            break;

        case VTPARSE_ACTION_PRINT:
            Console_PrintByte_Locked(pConsole, b);
            break;

        default:
            // Ignore
            break;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Read/Write
////////////////////////////////////////////////////////////////////////////////


// Writes the given byte sequence of characters to the console.
// \param pConsole the console
// \param pBytes the byte sequence
// \param nBytes the number of bytes to write
// \return the number of bytes writte; a negative error code if an error was encountered
ByteCount Console_Write(ConsoleRef _Nonnull pConsole, const Byte* _Nonnull pBytes, ByteCount nBytesToWrite)
{
    const unsigned char* pChars = (const unsigned char*) pBytes;
    const unsigned char* pCharsEnd = pChars + nBytesToWrite;

    Lock_Lock(&pConsole->lock);
    while (pChars < pCharsEnd) {
        const unsigned char by = *pChars++;

        vtparse_byte(&pConsole->vtparse, by);
    }
    Lock_Unlock(&pConsole->lock);

    //Lock_Lock(&pConsole->lock);
    //vtparse(&pConsole->vtparse, (const unsigned char*) pBytes, (int) nBytesToWrite);
    //Lock_Unlock(&pConsole->lock);

    return nBytesToWrite;
}

ByteCount Console_Read(ConsoleRef _Nonnull pConsole, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    HIDEvent evt;
    Int evtCount;
    ByteCount nBytesRead = 0;
    ErrorCode err = EOK;

    Lock_Lock(&pConsole->lock);

    // First check whether we got a partial key byte sequence sitting in our key
    // mapping buffer and copy that one out.
    while (nBytesRead < nBytesToRead && pConsole->keyMapper.count > 0) {
        pBuffer[nBytesRead++] = pConsole->keyMapper.buffer[pConsole->keyMapper.startIndex++];
        pConsole->keyMapper.count--;
    }


    // Now wait for events and map them to byte sequences if we stil got space
    // in the user provided buffer
    while (nBytesRead < nBytesToRead) {
        evtCount = 1;

        err = EventDriver_GetEvents(pConsole->pEventDriver, &evt, &evtCount, kTimeInterval_Infinity);
        if (err != EOK) {
            break;
        }

        if (evt.type != kHIDEventType_KeyDown) {
            continue;
        }


        pConsole->keyMapper.count = KeyMap_Map(pConsole->keyMapper.map, &evt.data.key, pConsole->keyMapper.buffer, pConsole->keyMapper.capacity);

        Int i = 0;
        while (nBytesRead < nBytesToRead && pConsole->keyMapper.count > 0) {
            pBuffer[nBytesRead++] = pConsole->keyMapper.buffer[i++];
            pConsole->keyMapper.count--;
        }

        if (pConsole->keyMapper.count > 0) {
            // We ran out of space in the buffer that the user gave us. Remember
            // which bytes we need to copy next time read() is called.
            pConsole->keyMapper.startIndex = i;
        }
    }

    Lock_Unlock(&pConsole->lock);
    
    return (err == EOK) ? nBytesRead : -err;
}
