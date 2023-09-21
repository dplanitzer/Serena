//
//  Console.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"

static void Console_ResetState_Locked(ConsoleRef _Nonnull pConsole);
static void Console_ClearScreen_Locked(Console* _Nonnull pConsole);
static void Console_MoveCursorTo_Locked(Console* _Nonnull pConsole, Int x, Int y);
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
    const Surface* pFramebuffer;

    try_null(pFramebuffer, GraphicsDriver_GetFramebuffer(pGDevice), ENODEV);
    try(kalloc_cleared(sizeof(Console), (Byte**) &pConsole));
    
    Lock_Init(&pConsole->lock);

    pConsole->pEventDriver = pEventDriver;
    pConsole->pGDevice = pGDevice;

    pConsole->cols = pFramebuffer->width / GLYPH_WIDTH;
    pConsole->rows = pFramebuffer->height / GLYPH_HEIGHT;

    vtparse_init(&pConsole->vtparse, Console_ParseInputBytes_Locked, pConsole);

    pConsole->keyMapper.map = (const KeyMap*) &gKeyMap_usa[0];
    pConsole->keyMapper.capacity = KeyMap_GetMaxOutputByteCount(pConsole->keyMapper.map);
    try(kalloc_cleared(pConsole->keyMapper.capacity, &pConsole->keyMapper.buffer));
    
    Console_ResetState_Locked(pConsole);
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
        Lock_Deinit(&pConsole->lock);
        kfree((Byte*)pConsole);
    }
}

static void Console_ResetState_Locked(ConsoleRef _Nonnull pConsole)
{
    Console_MoveCursorTo_Locked(pConsole, 0, 0);
    pConsole->flags = CONSOLE_FLAG_AUTOSCROLL_TO_BOTTOM;
    pConsole->lineBreakMode = kLineBreakMode_WrapCharacter;
    pConsole->tabWidth = 8;
}

// Returns the console bounds.
static Rect Console_GetBounds_Locked(ConsoleRef _Nonnull pConsole)
{
    return Rect_Make(0, 0, pConsole->cols, pConsole->rows);
}

// Clears the console screen.
// \param pConsole the console
static void Console_ClearScreen_Locked(ConsoleRef _Nonnull pConsole)
{
    pConsole->x = 0;
    pConsole->y = 0;
    GraphicsDriver_Clear(pConsole->pGDevice);
}

// Clears the specified line. Does not change the
// cursor position.
static void Console_ClearLine_Locked(ConsoleRef _Nonnull pConsole, Int y)
{
    const Rect bounds = Rect_Make(0, 0, pConsole->cols, pConsole->rows);
    const Rect r = Rect_Intersection(Rect_Make(0, y, pConsole->cols, 1), bounds);
    
    GraphicsDriver_FillRect(pConsole->pGDevice,
                            Rect_Make(r.x * GLYPH_WIDTH, r.y * GLYPH_HEIGHT, r.width * GLYPH_WIDTH, r.height * GLYPH_HEIGHT),
                            Color_MakeIndex(0));
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
static void Console_ScrollBy_Locked(ConsoleRef _Nonnull pConsole, Rect clipRect, Point dXY)
{
    if (dXY.x == 0 && dXY.y == 0) {
        return;
    }
    
    const Rect bounds = Rect_Make(0, 0, pConsole->cols, pConsole->rows);
    const Int hExposedWidth = __min(__abs(dXY.x), clipRect.width);
    const Int vExposedHeight = __min(__abs(dXY.y), clipRect.height);
    Rect copyRect, hClearRect, vClearRect;
    Point dstLoc;
    
    copyRect.x = (dXY.x < 0) ? clipRect.x : __min(clipRect.x + dXY.x, Rect_GetMaxX(clipRect));
    copyRect.y = (dXY.y < 0) ? clipRect.y : __min(clipRect.y + dXY.y, Rect_GetMaxY(clipRect));
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

    Console_CopyRect_Locked(pConsole, copyRect, dstLoc);
    Console_FillRect_Locked(pConsole, hClearRect, ' ');
    Console_FillRect_Locked(pConsole, vClearRect, ' ');
}

// Sets the console position. The next print() will start printing at this
// location.
// \param pConsole the console
// \param x the X position
// \param y the Y position
static void Console_MoveCursorTo_Locked(Console* _Nonnull pConsole, Int x, Int y)
{
    pConsole->x = __max(__min(x, pConsole->cols - 1), 0);
    pConsole->y = __max(__min(y, pConsole->rows - 1), 0);
}

// Moves the console position by the given delta values.
// \param pConsole the console
// \param dx the X delta
// \param dy the Y delta
static void Console_MoveCursor_Locked(ConsoleRef _Nonnull pConsole, Int dx, Int dy)
{
    Console_MoveCursorTo_Locked(pConsole, pConsole->x + dx, pConsole->y + dy);
}


////////////////////////////////////////////////////////////////////////////////
// Processing input bytes
////////////////////////////////////////////////////////////////////////////////

// Interprets the given byte as a character, maps it to a glyph and prints it.
// \param pConsole the console
// \param ch the character
static void Console_PrintByte_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    const Bool isAutoscrollEnabled = (pConsole->flags & CONSOLE_FLAG_AUTOSCROLL_TO_BOTTOM) != 0;
    
    if (pConsole->x >= pConsole->cols && pConsole->lineBreakMode == kLineBreakMode_WrapCharacter) {
        // wrap the line if wrap-by-character is active
        pConsole->x = 0;
        pConsole->y++;
    }

    if (pConsole->y >= pConsole->rows && isAutoscrollEnabled) {
        // auto scroll the console if we hit the bottom edge
        Console_ScrollBy_Locked(pConsole, Console_GetBounds_Locked(pConsole), Point_Make(0, 1));
        pConsole->y--;
    }
            
    if ((pConsole->x >= 0 && pConsole->x < pConsole->cols) && (pConsole->y >= 0 && pConsole->y < pConsole->rows)) {
        GraphicsDriver_BlitGlyph_8x8bw(pConsole->pGDevice, &font8x8_latin1[ch][0], pConsole->x, pConsole->y);
    }

    pConsole->x++;
}

// Interprets the given byte as a C0/C1 control character and either executes it or ignores it.
// \param pConsole the console
// \param ch the character
static void Console_ExecuteByte_C0_C1_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    const Bool isAutoscrollEnabled = (pConsole->flags & CONSOLE_FLAG_AUTOSCROLL_TO_BOTTOM) != 0;
    
    switch (ch) {
        case 0x07:  // BEL (Bell)
            // XXX flash screen?
            break;

        case 0x08:  // BS (Backspace)
            if (pConsole->x > 0) {
                // BS moves 1 cell to the left
                Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x, pConsole->y, pConsole->cols - pConsole->x, 1), Point_Make(pConsole->x - 1, pConsole->y));
                Console_FillRect_Locked(pConsole, Rect_Make(pConsole->cols - 1, pConsole->y, 1, 1), ' ');
                pConsole->x--;
            }
            break;

        case 0x09:  // HT (Tab)
            if (pConsole->tabWidth > 0) {
                // go to the next tab stop
                pConsole->x = (pConsole->x / pConsole->tabWidth + 1) * pConsole->tabWidth;
            
                if (pConsole->x >= pConsole->cols && pConsole->lineBreakMode == kLineBreakMode_WrapCharacter) {
                    // Wrap-by-character is enabled. Treat this like a newline aka move to the first tab stop in the next line
                    Console_ExecuteByte_C0_C1_Locked(pConsole, '\n');
                }
            }
            break;

        case 0x0a:  // LF (Line Feed)
            pConsole->x = 0;
            // fall through
            
        case 0x0b:  // VT (Vertical Tab)
            pConsole->y++;
            if (pConsole->y == pConsole->rows && isAutoscrollEnabled) {
                Console_ScrollBy_Locked(pConsole, Console_GetBounds_Locked(pConsole), Point_Make(0, 1));
                pConsole->y--;
            }
            break;
            
        case 0x0d:  // CR (Carriage Return)
            pConsole->x = 0;
            break;
                        
        case 0x0c:  // FF (Form Feed / New Page / Clear Screen)
            Console_ClearScreen_Locked(pConsole);
            break;
            
        case 0x7f:  // DEL (Delete)
            if (pConsole->x < pConsole->cols - 1) {
                // DEL does not change the position.
                Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x + 1, pConsole->y, pConsole->cols - (pConsole->x + 1), 1), Point_Make(pConsole->x, pConsole->y));
                Console_FillRect_Locked(pConsole, Rect_Make(pConsole->cols - 1, pConsole->y, 1, 1), ' ');
            }
            break;
            
        case 0x8d:  // RI (Reverse Line Feed)
            pConsole->y--;
            break;
            
        case 0x94:  // CCH (Cancel Character (replace the previous character with a space))
            if (pConsole->x > 0) {
                pConsole->x--;
                GraphicsDriver_BlitGlyph_8x8bw(pConsole->pGDevice, &font8x8_latin1[0x20][0], pConsole->x, pConsole->y);
            }
            break;
            
        default:
            // Ignore it
            break;
    }
}

//
// <https://vt100.net/emu/dec_ansi_parser>
// <https://en.wikipedia.org/wiki/ANSI_escape_code>
//

static Int get_csi_parameter(ConsoleRef _Nonnull pConsole, Int defValue)
{
    return (pConsole->vtparse.num_params > 0) ? pConsole->vtparse.params[0] : defValue;
}

static Int get_nth_csi_parameter(ConsoleRef _Nonnull pConsole, Int idx, Int defValue)
{
    return (pConsole->vtparse.num_params > idx) ? pConsole->vtparse.params[idx] : defValue;
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
            Console_MoveCursorTo_Locked(pConsole, get_csi_parameter(pConsole, 1), pConsole->y);
            break;

        case 'H':
        case 'f':
            Console_MoveCursorTo_Locked(pConsole, get_nth_csi_parameter(pConsole, 1, 1), get_nth_csi_parameter(pConsole, 0, 1));
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
