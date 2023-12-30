//
//  Console.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: ConsoleChannel
////////////////////////////////////////////////////////////////////////////////

void ConsoleChannel_deinit(ConsoleChannelRef _Nonnull self)
{
    kfree(self->buffer);
    self->buffer = NULL;
}

CLASS_METHODS(ConsoleChannel, IOChannel,
OVERRIDE_METHOD_IMPL(deinit, ConsoleChannel, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Console
////////////////////////////////////////////////////////////////////////////////

// Creates a new console object. This console will display its output on the
// provided graphics device.
// \param pEventDriver the event driver to provide keyboard input
// \param pGDevice the graphics device
// \return the console; NULL on failure
ErrorCode Console_Create(EventDriverRef _Nonnull pEventDriver, GraphicsDriverRef _Nonnull pGDevice, ConsoleRef _Nullable * _Nonnull pOutConsole)
{
    decl_try_err();
    Console* pConsole;
    User user = {kRootUserId, kRootGroupId};//XXX

    try(Object_Create(Console, &pConsole));
    
    Lock_Init(&pConsole->lock);

    pConsole->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    try(IOResource_Open(pConsole->eventDriver, NULL/*XXX*/, O_RDONLY, user, &pConsole->eventDriverChannel));

    pConsole->gdevice = Object_RetainAs(pGDevice, GraphicsDriver);

    pConsole->lineHeight = GLYPH_HEIGHT;
    pConsole->characterWidth = GLYPH_WIDTH;
    pConsole->backgroundColor = RGBColor_Make(0, 0, 0);
    pConsole->textColor = RGBColor_Make(0, 255, 0);


    // Initialize the ANSI escape sequence parser
    vtparse_init(&pConsole->vtparse, Console_ParseInputBytes_Locked, pConsole);


    // Allocate the text cursor (sprite)
    const Bool isInterlaced = ScreenConfiguration_IsInterlaced(GraphicsDriver_GetCurrentScreenConfiguration(pGDevice));
    const UInt16* textCursorPlanes[2];
    textCursorPlanes[0] = (isInterlaced) ? &gBlock4x4_Plane0[0] : &gBlock4x8_Plane0[0];
    textCursorPlanes[1] = (isInterlaced) ? &gBlock4x4_Plane0[1] : &gBlock4x8_Plane0[1];
    const Int textCursorWidth = (isInterlaced) ? gBlock4x4_Width : gBlock4x8_Width;
    const Int textCursorHeight = (isInterlaced) ? gBlock4x4_Height : gBlock4x8_Height;
    try(GraphicsDriver_AcquireSprite(pGDevice, textCursorPlanes, 0, 0, textCursorWidth, textCursorHeight, 0, &pConsole->textCursor));
    pConsole->isTextCursorVisible = false;


    // Allocate the text cursor blinking timer
    pConsole->isTextCursorBlinkerEnabled = false;
    pConsole->isTextCursorOn = false;
    pConsole->isTextCursorSingleCycleOn = false;
    try(Timer_Create(kTimeInterval_Zero, TimeInterval_MakeMilliseconds(500), DispatchQueueClosure_Make((Closure1Arg_Func)Console_OnTextCursorBlink, (Byte*)pConsole), &pConsole->textCursorBlinker));


    // Reset the console to the default configuration
    try(Console_ResetState_Locked(pConsole));


    // Clear the console screen
    Console_ClearScreen_Locked(pConsole);
    
    *pOutConsole = pConsole;
    return err;
    
catch:
    Object_Release(pConsole);
    *pOutConsole = NULL;
    return err;
}

// Deallocates the console.
// \param pConsole the console
void Console_deinit(ConsoleRef _Nonnull pConsole)
{
    Console_SetCursorBlinkingEnabled_Locked(pConsole, false);
    GraphicsDriver_RelinquishSprite(pConsole->gdevice, pConsole->textCursor);

    Timer_Destroy(pConsole->textCursorBlinker);
    pConsole->textCursorBlinker = NULL;
        
    TabStops_Deinit(&pConsole->hTabStops);
    TabStops_Deinit(&pConsole->vTabStops);
        
    Lock_Deinit(&pConsole->lock);

    Object_Release(pConsole->gdevice);
    pConsole->gdevice = NULL;

    if (pConsole->eventDriverChannel) {
        IOResource_Close(pConsole->eventDriver, pConsole->eventDriverChannel);
        Object_Release(pConsole->eventDriverChannel);
        pConsole->eventDriverChannel = NULL;
    }
    Object_Release(pConsole->eventDriver);
    pConsole->eventDriver = NULL;
}

static ErrorCode Console_ResetState_Locked(ConsoleRef _Nonnull pConsole)
{
    decl_try_err();
    const Surface* pFramebuffer;
    
    try_null(pFramebuffer, GraphicsDriver_GetFramebuffer(pConsole->gdevice), ENODEV);
    pConsole->bounds = Rect_Make(0, 0, pFramebuffer->width / pConsole->characterWidth, pFramebuffer->height / pConsole->lineHeight);
    pConsole->savedCursorPosition = Point_Zero;

    GraphicsDriver_SetCLUTEntry(pConsole->gdevice, 0, &pConsole->backgroundColor);
    GraphicsDriver_SetCLUTEntry(pConsole->gdevice, 1, &pConsole->textColor);
    GraphicsDriver_SetCLUTEntry(pConsole->gdevice, 17, &pConsole->textColor);
    GraphicsDriver_SetCLUTEntry(pConsole->gdevice, 18, &pConsole->textColor);
    GraphicsDriver_SetCLUTEntry(pConsole->gdevice, 19, &pConsole->textColor);

    TabStops_Deinit(&pConsole->hTabStops);
    try(TabStops_Init(&pConsole->hTabStops, __max(Rect_GetWidth(pConsole->bounds) / 8, 0), 8));

    TabStops_Deinit(&pConsole->vTabStops);
    try(TabStops_Init(&pConsole->vTabStops, 0, 0));

    Console_MoveCursorTo_Locked(pConsole, 0, 0);
    Console_SetCursorVisible_Locked(pConsole, true);
    Console_SetCursorBlinkingEnabled_Locked(pConsole, true);
    pConsole->lineBreakMode = kLineBreakMode_WrapCharacterAndScroll;

    return EOK;

catch:
    return err;
}

// Clears the console screen.
// \param pConsole the console
static void Console_ClearScreen_Locked(ConsoleRef _Nonnull pConsole)
{
    GraphicsDriver_Clear(pConsole->gdevice);
}

// Clears the specified line. Does not change the cursor position.
static void Console_ClearLine_Locked(ConsoleRef _Nonnull pConsole, Int y, ClearLineMode mode)
{
    if (Rect_Contains(pConsole->bounds, 0, y)) {
        Int left, right;

        switch (mode) {
            case kClearLineMode_Whole:
                left = 0;
                right = pConsole->bounds.right;
                break;

            case kClearLineMode_ToBeginning:
                left = 0;
                right = pConsole->x;
                break;

            case kClearLineMode_ToEnd:
                left = pConsole->x;
                right = pConsole->bounds.right;
                break;

            default:
                abort();
        }

        GraphicsDriver_FillRect(pConsole->gdevice,
                            Rect_Make(left * pConsole->characterWidth, y * pConsole->lineHeight, right * pConsole->characterWidth, (y + 1) * pConsole->lineHeight),
                            Color_MakeIndex(0));
    }
}

// Copies the content of 'srcRect' to 'dstLoc'. Does not change the cursor
// position.
static void Console_CopyRect_Locked(ConsoleRef _Nonnull pConsole, Rect srcRect, Point dstLoc)
{
    GraphicsDriver_CopyRect(pConsole->gdevice,
                            Rect_Make(srcRect.left * pConsole->characterWidth, srcRect.top * pConsole->lineHeight, srcRect.right * pConsole->characterWidth, srcRect.bottom * pConsole->lineHeight),
                            Point_Make(dstLoc.x * pConsole->characterWidth, dstLoc.y * pConsole->lineHeight));
}

// Fills the content of 'rect' with the character 'ch'. Does not change the
// cursor position.
static void Console_FillRect_Locked(ConsoleRef _Nonnull pConsole, Rect rect, Character ch)
{
    const Rect r = Rect_Intersection(rect, pConsole->bounds);

    if (ch == ' ') {
        GraphicsDriver_FillRect(pConsole->gdevice,
                                Rect_Make(r.left * pConsole->characterWidth, r.top * pConsole->lineHeight, r.right * pConsole->characterWidth, r.bottom * pConsole->lineHeight),
                                Color_MakeIndex(0));
    }
    else if (ch < 32 || ch == 127) {
        // Control characters -> do nothing
    }
    else {
        for (Int y = r.top; y < r.bottom; y++) {
            for (Int x = r.left; x < r.right; x++) {
                GraphicsDriver_BlitGlyph_8x8bw(pConsole->gdevice, &font8x8_latin1[ch][0], x, y);
            }
        }
    }
}

// Scrolls the content of the console screen. 'clipRect' defines a viewport
// through which a virtual document is visible. This viewport is scrolled by
// 'dX' / 'dY' character cells. Positive values move the viewport down/right
// (and scroll the virtual document up/left) and negative values move the
// viewport up/left (and scroll the virtual document down/right).
static void Console_ScrollBy_Locked(ConsoleRef _Nonnull pConsole, Int dX, Int dY)
{
    const Rect clipRect = pConsole->bounds;
    const Int absDx = __abs(dX), absDy = __abs(dY);

    if (absDx < Rect_GetWidth(clipRect) && absDy < Rect_GetHeight(clipRect)) {
        if (absDx > 0 || absDy > 0) {
            Rect copyRect;
            Point dstLoc;

            copyRect.left = (dX < 0) ? clipRect.left : clipRect.left + absDx;
            copyRect.top = (dY < 0) ? clipRect.top : clipRect.top + absDy;
            copyRect.right = (dX < 0) ? clipRect.right - absDx : clipRect.right;
            copyRect.bottom = (dY < 0) ? clipRect.bottom - absDy : clipRect.bottom;

            dstLoc.x = (dX < 0) ? clipRect.left + absDx : clipRect.left;
            dstLoc.y = (dY < 0) ? clipRect.top + absDy : clipRect.top;

            Console_CopyRect_Locked(pConsole, copyRect, dstLoc);
        }


        if (absDy > 0) {
            Rect vClearRect;

            vClearRect.left = clipRect.left;
            vClearRect.top = (dY < 0) ? clipRect.top : clipRect.bottom - absDy;
            vClearRect.right = clipRect.right;
            vClearRect.bottom = (dY < 0) ? clipRect.top + absDy : clipRect.bottom;
            Console_FillRect_Locked(pConsole, vClearRect, ' ');
        }


        if (absDx > 0) {
            Rect hClearRect;

            hClearRect.left = (dX < 0) ? clipRect.left : clipRect.right - absDx;
            hClearRect.top = (dY < 0) ? clipRect.top + absDy : clipRect.top;
            hClearRect.right = (dX < 0) ? clipRect.left + absDx : clipRect.right;
            hClearRect.bottom = (dY < 0) ? clipRect.bottom : clipRect.bottom - absDy;

            Console_FillRect_Locked(pConsole, hClearRect, ' ');
        }
    }
    else {
        Console_ClearScreen_Locked(pConsole);
    }
}

static void Console_DeleteLines_Locked(ConsoleRef _Nonnull pConsole, Int nLines)
{
    if (nLines > 0) {
        const Int copyFromLine = pConsole->y + nLines;
        const Int linesToCopy = __max(pConsole->bounds.bottom - copyFromLine, 0);
        const Int clearFromLine = pConsole->y + linesToCopy;
        const Int linesToClear = __max(pConsole->bounds.bottom - clearFromLine, 0);

        if (linesToCopy > 0) {
            Console_CopyRect_Locked(pConsole,
                Rect_Make(0, copyFromLine, pConsole->bounds.right, copyFromLine + linesToCopy),
                Point_Make(0, pConsole->y));
        }

        if (linesToClear > 0) {
            Console_FillRect_Locked(pConsole,
                Rect_Make(0, clearFromLine, pConsole->bounds.right, pConsole->bounds.bottom),
                ' ');
        }
    }
}

static void Console_OnTextCursorBlink(Console* _Nonnull pConsole)
{
    Lock_Lock(&pConsole->lock);
    
    pConsole->isTextCursorOn = !pConsole->isTextCursorOn;
    if (pConsole->isTextCursorVisible) {
        GraphicsDriver_SetSpriteVisible(pConsole->gdevice, pConsole->textCursor, pConsole->isTextCursorOn || pConsole->isTextCursorSingleCycleOn);
    }
    pConsole->isTextCursorSingleCycleOn = false;

    Lock_Unlock(&pConsole->lock);
}

static void Console_UpdateCursorVisibilityAndRestartBlinking_Locked(Console* _Nonnull pConsole)
{
    if (pConsole->isTextCursorVisible) {
        // Changing the visibility to on should restart the blinking timer if
        // blinking is on too so that we always start out with a cursor-on phase
        DispatchQueue_RemoveTimer(gMainDispatchQueue, pConsole->textCursorBlinker);
        GraphicsDriver_SetSpriteVisible(pConsole->gdevice, pConsole->textCursor, true);
        pConsole->isTextCursorOn = false;
        pConsole->isTextCursorSingleCycleOn = false;

        if (pConsole->isTextCursorBlinkerEnabled) {
            try_bang(DispatchQueue_DispatchTimer(gMainDispatchQueue, pConsole->textCursorBlinker));
        }
    } else {
        // Make sure that the text cursor and blinker are off
        DispatchQueue_RemoveTimer(gMainDispatchQueue, pConsole->textCursorBlinker);
        GraphicsDriver_SetSpriteVisible(pConsole->gdevice, pConsole->textCursor, false);
        pConsole->isTextCursorOn = false;
        pConsole->isTextCursorSingleCycleOn = false;
    }
}

static void Console_SetCursorBlinkingEnabled_Locked(Console* _Nonnull pConsole, Bool isEnabled)
{
    if (pConsole->isTextCursorBlinkerEnabled != isEnabled) {
        pConsole->isTextCursorBlinkerEnabled = isEnabled;
        Console_UpdateCursorVisibilityAndRestartBlinking_Locked(pConsole);
    }
}

static void Console_SetCursorVisible_Locked(Console* _Nonnull pConsole, Bool isVisible)
{
    if (pConsole->isTextCursorVisible != isVisible) {
        pConsole->isTextCursorVisible = isVisible;
        Console_UpdateCursorVisibilityAndRestartBlinking_Locked(pConsole);
    }
}

static void Console_CursorDidMove_Locked(Console* _Nonnull pConsole)
{
    GraphicsDriver_SetSpritePosition(pConsole->gdevice, pConsole->textCursor, pConsole->x * pConsole->characterWidth, pConsole->y * pConsole->lineHeight);
    // Temporarily force the cursor to be visible, but without changing the text
    // cursor visibility state officially. We just want to make sure that the
    // cursor is on when the user types a character. This however should not
    // change anything about the blinking phase and frequency.
    if (!pConsole->isTextCursorSingleCycleOn && !pConsole->isTextCursorOn && pConsole->isTextCursorBlinkerEnabled && pConsole->isTextCursorVisible) {
        pConsole->isTextCursorSingleCycleOn = true;
        GraphicsDriver_SetSpriteVisible(pConsole->gdevice, pConsole->textCursor, true);
    }
}

// Sets the console position. The next print() will start printing at this
// location.
// \param pConsole the console
// \param x the X position
// \param y the Y position
static void Console_MoveCursorTo_Locked(Console* _Nonnull pConsole, Int x, Int y)
{
    pConsole->x = __max(__min(x, pConsole->bounds.right - 1), 0);
    pConsole->y = __max(__min(y, pConsole->bounds.bottom - 1), 0);
    Console_CursorDidMove_Locked(pConsole);
}

// Moves the console position by the given delta values.
// \param pConsole the console
// \param dx the X delta
// \param dy the Y delta
static void Console_MoveCursor_Locked(ConsoleRef _Nonnull pConsole, Int dx, Int dy)
{
    const Int eX = pConsole->bounds.right - 1;
    const Int eY = pConsole->bounds.bottom - 1;
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
    Console_CursorDidMove_Locked(pConsole);
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
    GraphicsDriver_BlitGlyph_8x8bw(pConsole->gdevice, &font8x8_latin1[ch][0], pConsole->x, pConsole->y);
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
        Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x, pConsole->y, pConsole->bounds.right, pConsole->y + 1), Point_Make(pConsole->x - 1, pConsole->y));
        Console_FillRect_Locked(pConsole, Rect_Make(pConsole->bounds.right - 1, pConsole->y, pConsole->bounds.right, pConsole->y + 1), ' ');
        Console_MoveCursor_Locked(pConsole, -1, 0);
    }
}

static void Console_Execute_HT_Locked(ConsoleRef _Nonnull pConsole)
{
    Console_MoveCursorTo_Locked(pConsole, TabStops_GetNextStop(&pConsole->hTabStops, pConsole->x, Rect_GetWidth(pConsole->bounds)), pConsole->y);
}

static void Console_Execute_HTS_Locked(ConsoleRef _Nonnull pConsole)
{
    TabStops_InsertStop(&pConsole->hTabStops, pConsole->x);
}

static void Console_Execute_VT_Locked(ConsoleRef _Nonnull pConsole)
{
    if (pConsole->vTabStops.count > 0) {
        Console_MoveCursorTo_Locked(pConsole, pConsole->x, TabStops_GetNextStop(&pConsole->vTabStops, pConsole->y, Rect_GetHeight(pConsole->bounds)));
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
    if (pConsole->x < pConsole->bounds.right - 1) {
        // DEL does not change the position.
        Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x + 1, pConsole->y, pConsole->bounds.right, pConsole->y + 1), Point_Make(pConsole->x, pConsole->y));
        Console_FillRect_Locked(pConsole, Rect_Make(pConsole->bounds.right - 1, pConsole->y, pConsole->bounds.right, pConsole->y + 1), ' ');
    }
}

static void Console_Execute_CCH_Locked(ConsoleRef _Nonnull pConsole)
{
    if (pConsole->x > 0) {
        Console_MoveCursor_Locked(pConsole, -1, 0);
        GraphicsDriver_BlitGlyph_8x8bw(pConsole->gdevice, &font8x8_latin1[0x20][0], pConsole->x, pConsole->y);
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

static Bool has_private_use_char(ConsoleRef _Nonnull pConsole, Character ch)
{
    return pConsole->vtparse.num_intermediate_chars > 0 && pConsole->vtparse.intermediate_chars[0] == ch;
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
        Rect_Make(pConsole->x, pConsole->y, __min(pConsole->x + nChars, pConsole->bounds.right), pConsole->y + 1),
        ' ');
}

static void Console_Execute_CSI_QuestMark_h_Locked(ConsoleRef _Nonnull pConsole, Int param)
{
    switch (param) {
        case 25:
            Console_SetCursorVisible_Locked(pConsole, true);
            break;

        default:
            // Ignore
            break;
    }
}

static void Console_Execute_CSI_QuestMark_l_Locked(ConsoleRef _Nonnull pConsole, Int param)
{
    switch (param) {
        case 25:
            Console_SetCursorVisible_Locked(pConsole, false);
            break;

        default:
            // Ignore
            break;
    }
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
            Console_MoveCursor_Locked(pConsole, TabStops_GetNextNthStop(&pConsole->hTabStops, pConsole->x, get_csi_parameter(pConsole, 1), Rect_GetWidth(pConsole->bounds)), 0);
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
            Console_MoveCursor_Locked(pConsole, 0, TabStops_GetNextNthStop(&pConsole->vTabStops, pConsole->y, get_csi_parameter(pConsole, 1), Rect_GetHeight(pConsole->bounds)));
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

        case 'h':
            if (has_private_use_char(pConsole, '?')) {
                Console_Execute_CSI_QuestMark_h_Locked(pConsole, get_csi_parameter(pConsole, 0));
            }
            break;

        case 'l':
            if (has_private_use_char(pConsole, '?')) {
                Console_Execute_CSI_QuestMark_l_Locked(pConsole, get_csi_parameter(pConsole, 0));
            }
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

ErrorCode Console_open(ConsoleRef _Nonnull pConsole, InodeRef _Nonnull _Locked pNode, UInt mode, User user, ConsoleChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ConsoleChannelRef pChannel;
    const KeyMap* pKeyMap = (const KeyMap*) &gKeyMap_usa[0];
    const ByteCount keyMapSize = KeyMap_GetMaxOutputByteCount(pKeyMap);

    try(IOChannel_AbstractCreate(&kConsoleChannelClass, (IOResourceRef) pConsole, mode, (IOChannelRef*)&pChannel));
    try(kalloc(keyMapSize, (void**)&pChannel->buffer));
    pChannel->map = pKeyMap;
    pChannel->capacity = keyMapSize;
    pChannel->count = 0;
    pChannel->startIndex = 0;

catch:
    *pOutChannel = pChannel;
    return err;
}

ErrorCode Console_dup(ConsoleRef _Nonnull pConsole, ConsoleChannelRef _Nonnull pInChannel, ConsoleChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ConsoleChannelRef pNewChannel;

    try(IOChannel_AbstractCreateCopy((IOChannelRef)pInChannel, (IOChannelRef*)&pNewChannel));
    try(kalloc(pInChannel->capacity, (void**)&pNewChannel->buffer));
    pNewChannel->map = pInChannel->map;
    pNewChannel->capacity = pInChannel->capacity;
    pNewChannel->count = 0;
    pNewChannel->startIndex = 0;

catch:
    *pOutChannel = pNewChannel;
    return err;

}

ByteCount Console_read(ConsoleRef _Nonnull pConsole, ConsoleChannelRef _Nonnull pChannel, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    HIDEvent evt;
    Int evtCount;
    ByteCount nBytesRead = 0;
    ErrorCode err = EOK;

    Lock_Lock(&pConsole->lock);

    // First check whether we got a partial key byte sequence sitting in our key
    // mapping buffer and copy that one out.
    while (nBytesRead < nBytesToRead && pChannel->count > 0) {
        pBuffer[nBytesRead++] = pChannel->buffer[pChannel->startIndex++];
        pChannel->count--;
    }


    // Now wait for events and map them to byte sequences if we still got space
    // in the user provided buffer
    while (nBytesRead < nBytesToRead) {
        ByteCount nEvtBytesRead;
        evtCount = 1;

        // Drop the console lock while getting an event since the get events call
        // may block and holding the lock while being blocked for a potentially
        // long time would prevent any other process from working with the
        // console
        Lock_Unlock(&pConsole->lock);
        nEvtBytesRead = IOChannel_Read(pConsole->eventDriverChannel, (Byte*) &evt, sizeof(evt));
        Lock_Lock(&pConsole->lock);
        // XXX we are currently assuming here that no relevant console state has
        // XXX changed while we didn't hold the lock. Confirm that this is okay
        if (nEvtBytesRead < 0) {
            err = (ErrorCode) -nEvtBytesRead;
            break;
        }

        if (evt.type != kHIDEventType_KeyDown) {
            continue;
        }


        pChannel->count = KeyMap_Map(pChannel->map, &evt.data.key, pChannel->buffer, pChannel->capacity);

        Int i = 0;
        while (nBytesRead < nBytesToRead && pChannel->count > 0) {
            pBuffer[nBytesRead++] = pChannel->buffer[i++];
            pChannel->count--;
        }

        if (pChannel->count > 0) {
            // We ran out of space in the buffer that the user gave us. Remember
            // which bytes we need to copy next time read() is called.
            pChannel->startIndex = i;
        }
    }

    Lock_Unlock(&pConsole->lock);
    
    return (err == EOK) ? nBytesRead : -err;
}

// Writes the given byte sequence of characters to the console.
// \param pConsole the console
// \param pBytes the byte sequence
// \param nBytes the number of bytes to write
// \return the number of bytes written; a negative error code if an error was encountered
ByteCount Console_write(ConsoleRef _Nonnull pConsole, ConsoleChannelRef _Nonnull pChannel, const Byte* _Nonnull pBytes, ByteCount nBytesToWrite)
{
    const unsigned char* pChars = (const unsigned char*) pBytes;
    const unsigned char* pCharsEnd = pChars + nBytesToWrite;

    Lock_Lock(&pConsole->lock);
    while (pChars < pCharsEnd) {
        const unsigned char by = *pChars++;

        vtparse_byte(&pConsole->vtparse, by);
    }
    Lock_Unlock(&pConsole->lock);

    return nBytesToWrite;
}


CLASS_METHODS(Console, IOResource,
OVERRIDE_METHOD_IMPL(open, Console, IOResource)
OVERRIDE_METHOD_IMPL(dup, Console, IOResource)
OVERRIDE_METHOD_IMPL(read, Console, IOResource)
OVERRIDE_METHOD_IMPL(write, Console, IOResource)
OVERRIDE_METHOD_IMPL(deinit, Console, Object)
);
