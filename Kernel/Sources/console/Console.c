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

CLASS_METHODS(ConsoleChannel, IOChannel,
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
    pConsole->compatibilityMode = kCompatibilityMode_ANSI;


    // Initialize the ANSI escape sequence parser
    vtparser_init(&pConsole->vtparser, (vt52parse_callback_t)Console_VT52_ParseByte_Locked, (vt500parse_callback_t)Console_VT102_ParseByte_Locked, pConsole);


    // Allocate the text cursor (sprite)
    const Bool isInterlaced = ScreenConfiguration_IsInterlaced(GraphicsDriver_GetCurrentScreenConfiguration(pGDevice));
    const UInt16* textCursorPlanes[2];
    textCursorPlanes[0] = (isInterlaced) ? &gBlock4x4_Plane0[0] : &gBlock4x8_Plane0[0];
    textCursorPlanes[1] = (isInterlaced) ? &gBlock4x4_Plane0[1] : &gBlock4x8_Plane0[1];
    const Int textCursorWidth = (isInterlaced) ? gBlock4x4_Width : gBlock4x8_Width;
    const Int textCursorHeight = (isInterlaced) ? gBlock4x4_Height : gBlock4x8_Height;
    try(GraphicsDriver_AcquireSprite(pGDevice, textCursorPlanes, 0, 0, textCursorWidth, textCursorHeight, 0, &pConsole->textCursor));
    pConsole->flags.isTextCursorVisible = false;


    // Allocate the text cursor blinking timer
    pConsole->flags.isTextCursorBlinkerEnabled = false;
    pConsole->flags.isTextCursorOn = false;
    pConsole->flags.isTextCursorSingleCycleOn = false;
    try(Timer_Create(kTimeInterval_Zero, TimeInterval_MakeMilliseconds(500), DispatchQueueClosure_Make((Closure1Arg_Func)Console_OnTextCursorBlink, (Byte*)pConsole), &pConsole->textCursorBlinker));


    // Reset the console to the default configuration
    try(Console_ResetState_Locked(pConsole));


    // Clear the console screen
    Console_ClearScreen_Locked(pConsole, kClearScreenMode_WhileAndScrollback);
    
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

ErrorCode Console_ResetState_Locked(ConsoleRef _Nonnull pConsole)
{
    decl_try_err();
    const Surface* pFramebuffer;
    
    try_null(pFramebuffer, GraphicsDriver_GetFramebuffer(pConsole->gdevice), ENODEV);
    pConsole->bounds = Rect_Make(0, 0, pFramebuffer->width / pConsole->characterWidth, pFramebuffer->height / pConsole->lineHeight);
    pConsole->savedCursorState.x = 0;
    pConsole->savedCursorState.y = 0;

    GraphicsDriver_SetCLUTEntry(pConsole->gdevice, 0, &pConsole->backgroundColor);
    GraphicsDriver_SetCLUTEntry(pConsole->gdevice, 1, &pConsole->textColor);
    GraphicsDriver_SetCLUTEntry(pConsole->gdevice, 17, &pConsole->textColor);
    GraphicsDriver_SetCLUTEntry(pConsole->gdevice, 18, &pConsole->textColor);
    GraphicsDriver_SetCLUTEntry(pConsole->gdevice, 19, &pConsole->textColor);

    TabStops_Deinit(&pConsole->hTabStops);
    try(TabStops_Init(&pConsole->hTabStops, __max(Rect_GetWidth(pConsole->bounds) / 8, 0), 8));

    Console_MoveCursorTo_Locked(pConsole, 0, 0);
    Console_SetCursorVisible_Locked(pConsole, true);
    Console_SetCursorBlinkingEnabled_Locked(pConsole, true);
    pConsole->flags.isAutoWrapEnabled = true;
    pConsole->flags.isInsertionMode = false;

    return EOK;

catch:
    return err;
}

// Switches the console to the given compatibility mode
void Console_SetCompatibilityMode(ConsoleRef _Nonnull pConsole, CompatibilityMode mode)
{
    vtparser_mode_t vtmode;

    switch (mode) {
        case kCompatibilityMode_VT52:
            vtmode = VTPARSER_MODE_VT52;
            pConsole->flags.isInsertionMode = false;
            pConsole->flags.isAutoWrapEnabled = false;
            break;

        case kCompatibilityMode_VT52_AtariExtensions:
            vtmode = VTPARSER_MODE_VT52_ATARI;
            pConsole->flags.isInsertionMode = false;
            pConsole->flags.isAutoWrapEnabled = false;
            break;

        case kCompatibilityMode_VT102:
            vtmode = VTPARSER_MODE_VT102;
            break;

        default:
            abort();
            break;
    }

    vtparser_set_mode(&pConsole->vtparser, vtmode);
    pConsole->compatibilityMode = mode;
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
        Console_ClearScreen_Locked(pConsole, kClearScreenMode_Whole);
    }
}

// Clears the console screen.
// \param pConsole the console
// \param mode the clear screen mode
void Console_ClearScreen_Locked(ConsoleRef _Nonnull pConsole, ClearScreenMode mode)
{
    switch (mode) {
        case kClearScreenMode_ToEnd:
            Console_FillRect_Locked(pConsole, Rect_Make(pConsole->x, pConsole->y, pConsole->bounds.right, pConsole->y + 1), ' ');
            Console_FillRect_Locked(pConsole, Rect_Make(0, pConsole->y + 1, pConsole->bounds.right, pConsole->bounds.bottom), ' ');
            break;

        case kClearScreenMode_ToBeginning:
            Console_FillRect_Locked(pConsole, Rect_Make(0, pConsole->y, pConsole->x, pConsole->y + 1), ' ');
            Console_FillRect_Locked(pConsole, Rect_Make(0, 0, pConsole->bounds.right, pConsole->y - 1), ' ');
            break;

        case kClearScreenMode_Whole:
        case kClearScreenMode_WhileAndScrollback:
            GraphicsDriver_Clear(pConsole->gdevice);
            break;

        default:
            // Ignore
            break;
    }
}

// Clears the specified line. Does not change the cursor position.
void Console_ClearLine_Locked(ConsoleRef _Nonnull pConsole, Int y, ClearLineMode mode)
{
    if (Rect_Contains(pConsole->bounds, 0, y)) {
        Int left, right;

        switch (mode) {
            case kClearLineMode_ToEnd:
                left = pConsole->x;
                right = pConsole->bounds.right;
                break;

            case kClearLineMode_ToBeginning:
                left = 0;
                right = pConsole->x;
                break;

            case kClearLineMode_Whole:
                left = 0;
                right = pConsole->bounds.right;
                break;

            default:
                // Ignore
                return;
        }

        Console_FillRect_Locked(pConsole, Rect_Make(left, y, right, y + 1), ' ');
    }
}

void Console_SaveCursorState_Locked(ConsoleRef _Nonnull pConsole)
{
    pConsole->savedCursorState.x = pConsole->x;
    pConsole->savedCursorState.y = pConsole->y;
}

void Console_RestoreCursorState_Locked(ConsoleRef _Nonnull pConsole)
{
    Console_MoveCursorTo_Locked(pConsole, pConsole->savedCursorState.x, pConsole->savedCursorState.y);
}

static void Console_OnTextCursorBlink(ConsoleRef _Nonnull pConsole)
{
    Lock_Lock(&pConsole->lock);
    
    pConsole->flags.isTextCursorOn = !pConsole->flags.isTextCursorOn;
    if (pConsole->flags.isTextCursorVisible) {
        GraphicsDriver_SetSpriteVisible(pConsole->gdevice, pConsole->textCursor, pConsole->flags.isTextCursorOn || pConsole->flags.isTextCursorSingleCycleOn);
    }
    pConsole->flags.isTextCursorSingleCycleOn = false;

    Lock_Unlock(&pConsole->lock);
}

static void Console_UpdateCursorVisibilityAndRestartBlinking_Locked(Console* _Nonnull pConsole)
{
    if (pConsole->flags.isTextCursorVisible) {
        // Changing the visibility to on should restart the blinking timer if
        // blinking is on too so that we always start out with a cursor-on phase
        DispatchQueue_RemoveTimer(gMainDispatchQueue, pConsole->textCursorBlinker);
        GraphicsDriver_SetSpriteVisible(pConsole->gdevice, pConsole->textCursor, true);
        pConsole->flags.isTextCursorOn = false;
        pConsole->flags.isTextCursorSingleCycleOn = false;

        if (pConsole->flags.isTextCursorBlinkerEnabled) {
            try_bang(DispatchQueue_DispatchTimer(gMainDispatchQueue, pConsole->textCursorBlinker));
        }
    } else {
        // Make sure that the text cursor and blinker are off
        DispatchQueue_RemoveTimer(gMainDispatchQueue, pConsole->textCursorBlinker);
        GraphicsDriver_SetSpriteVisible(pConsole->gdevice, pConsole->textCursor, false);
        pConsole->flags.isTextCursorOn = false;
        pConsole->flags.isTextCursorSingleCycleOn = false;
    }
}

void Console_SetCursorBlinkingEnabled_Locked(Console* _Nonnull pConsole, Bool isEnabled)
{
    if (pConsole->flags.isTextCursorBlinkerEnabled != isEnabled) {
        pConsole->flags.isTextCursorBlinkerEnabled = isEnabled;
        Console_UpdateCursorVisibilityAndRestartBlinking_Locked(pConsole);
    }
}

void Console_SetCursorVisible_Locked(Console* _Nonnull pConsole, Bool isVisible)
{
    if (pConsole->flags.isTextCursorVisible != isVisible) {
        pConsole->flags.isTextCursorVisible = isVisible;
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
    if (!pConsole->flags.isTextCursorSingleCycleOn && !pConsole->flags.isTextCursorOn && pConsole->flags.isTextCursorBlinkerEnabled && pConsole->flags.isTextCursorVisible) {
        pConsole->flags.isTextCursorSingleCycleOn = true;
        GraphicsDriver_SetSpriteVisible(pConsole->gdevice, pConsole->textCursor, true);
    }
}

// Moves the console position by the given delta values.
// \param pConsole the console
// \param mode how the cursor movement should be handled if it tries to go past the margins
// \param dx the X delta
// \param dy the Y delta
void Console_MoveCursor_Locked(ConsoleRef _Nonnull pConsole, CursorMovement mode, Int dx, Int dy)
{
    if (dx == 0 && dy == 0) {
        return;
    }
    
    const Int aX = 0;
    const Int aY = 0;
    const Int eX = pConsole->bounds.right - 1;
    const Int eY = pConsole->bounds.bottom - 1;
    Int x = pConsole->x + dx;
    Int y = pConsole->y + dy;

    switch (mode) {
        case kCursorMovement_Clamp:
            x = __max(__min(x, eX), aX);
            y = __max(__min(y, eY), aY);
            break;

        case kCursorMovement_AutoWrap:
            if (x < aX) {
                x = aX;
            }
            else if (x > eX) {
                x = aX;
                y++;
            }

            if (y < aY) {
                Console_ScrollBy_Locked(pConsole, 0, y);
                y = aY;
            }
            else if (y > eY) {
                Console_ScrollBy_Locked(pConsole, 0, y - eY);
                y = eY;
            }
            break;

        case kCursorMovement_AutoScroll:
            x = __max(__min(x, eX), aX);

            if (y < aY) {
                Console_ScrollBy_Locked(pConsole, 0, y);
                y = aY;
            }
            else if (y > eY) {
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

// Sets the console position. The next print() will start printing at this
// location.
// \param pConsole the console
// \param x the X position
// \param y the Y position
void Console_MoveCursorTo_Locked(Console* _Nonnull pConsole, Int x, Int y)
{
    Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, x - pConsole->x, y - pConsole->y);
}


////////////////////////////////////////////////////////////////////////////////
// Processing input bytes
////////////////////////////////////////////////////////////////////////////////

// Interprets the given byte as a character, maps it to a glyph and prints it.
// \param pConsole the console
// \param ch the character
void Console_PrintByte_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    // The cursor position is always valid and inside the framebuffer
    if (pConsole->flags.isInsertionMode) {
        Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x, pConsole->y, pConsole->bounds.right - 1, pConsole->y + 1), Point_Make(pConsole->x + 1, pConsole->y));
    }

    GraphicsDriver_BlitGlyph_8x8bw(pConsole->gdevice, &font8x8_latin1[ch][0], pConsole->x, pConsole->y);
    Console_MoveCursor_Locked(pConsole, (pConsole->flags.isAutoWrapEnabled) ? kCursorMovement_AutoWrap : kCursorMovement_Clamp, 1, 0);
}

void Console_Execute_BEL_Locked(ConsoleRef _Nonnull pConsole)
{
    // XXX implement me
    // XXX flash screen?
}

void Console_Execute_HT_Locked(ConsoleRef _Nonnull pConsole)
{
    Console_MoveCursorTo_Locked(pConsole, TabStops_GetNextStop(&pConsole->hTabStops, pConsole->x, Rect_GetWidth(pConsole->bounds)), pConsole->y);
}

// Line feed may be IND or NEL depending on a setting (that doesn't exist yet)
void Console_Execute_LF_Locked(ConsoleRef _Nonnull pConsole)
{
    Console_MoveCursor_Locked(pConsole, kCursorMovement_AutoScroll, -pConsole->x, 1);
}

void Console_Execute_BS_Locked(ConsoleRef _Nonnull pConsole)
{
    if (pConsole->x > 0) {
        // BS moves 1 cell to the left. It moves all characters in the line and
        // to the right of the cursor one column to the left if insertion mode
        // is enabled.
        if (pConsole->flags.isInsertionMode) {
            Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x, pConsole->y, pConsole->bounds.right, pConsole->y + 1), Point_Make(pConsole->x - 1, pConsole->y));
            Console_FillRect_Locked(pConsole, Rect_Make(pConsole->bounds.right - 1, pConsole->y, pConsole->bounds.right, pConsole->y + 1), ' ');
        }
        Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, -1, 0);
    }
}

void Console_Execute_DEL_Locked(ConsoleRef _Nonnull pConsole)
{
    if (pConsole->x < pConsole->bounds.right - 1) {
        // DEL does not change the position.
        Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x + 1, pConsole->y, pConsole->bounds.right, pConsole->y + 1), Point_Make(pConsole->x, pConsole->y));
        Console_FillRect_Locked(pConsole, Rect_Make(pConsole->bounds.right - 1, pConsole->y, pConsole->bounds.right, pConsole->y + 1), ' ');
    }
}

void Console_Execute_DCH_Locked(ConsoleRef _Nonnull pConsole, Int nChars)
{
    Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x + nChars, pConsole->y, pConsole->bounds.right - nChars, pConsole->y + 1), Point_Make(pConsole->x, pConsole->y));
    Console_FillRect_Locked(pConsole, Rect_Make(pConsole->bounds.right - nChars, pConsole->y, pConsole->bounds.right, pConsole->y + 1), ' ');
}

void Console_Execute_IL_Locked(ConsoleRef _Nonnull pConsole, Int nLines)
{
    if (pConsole->y < pConsole->bounds.bottom) {
        Console_CopyRect_Locked(pConsole, Rect_Make(0, pConsole->y + 1, pConsole->bounds.right, pConsole->bounds.bottom - nLines), Point_Make(pConsole->x, pConsole->y + 2));
        Console_FillRect_Locked(pConsole, Rect_Make(0, pConsole->y + 1, pConsole->bounds.right, pConsole->y + 1 + nLines), ' ');
    }
}

void Console_Execute_DL_Locked(ConsoleRef _Nonnull pConsole, Int nLines)
{
    Console_CopyRect_Locked(pConsole, Rect_Make(0, pConsole->y + 1, pConsole->bounds.right, pConsole->bounds.bottom - nLines), Point_Make(pConsole->x, pConsole->y));
    Console_FillRect_Locked(pConsole, Rect_Make(0, pConsole->bounds.bottom - nLines, pConsole->bounds.right, pConsole->bounds.bottom), ' ');
}


////////////////////////////////////////////////////////////////////////////////
// Read/Write
////////////////////////////////////////////////////////////////////////////////

ErrorCode Console_open(ConsoleRef _Nonnull pConsole, InodeRef _Nonnull _Locked pNode, UInt mode, User user, ConsoleChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ConsoleChannelRef pChannel;
    const KeyMap* pKeyMap = (const KeyMap*) gKeyMap_usa;

    try(IOChannel_AbstractCreate(&kConsoleChannelClass, (IOResourceRef) pConsole, mode, (IOChannelRef*)&pChannel));
    Bytes_ClearRange(pChannel->buffer, kKeyMap_MaxByteSequenceLength);
    pChannel->map = pKeyMap;
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
    Bytes_ClearRange(pNewChannel->buffer, kKeyMap_MaxByteSequenceLength);
    pNewChannel->map = pInChannel->map;
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
        // Drop the console lock while getting an event since the get events call
        // may block and holding the lock while being blocked for a potentially
        // long time would prevent any other process from working with the
        // console
        Lock_Unlock(&pConsole->lock);
        const ByteCount nEvtBytesRead = IOChannel_Read(pConsole->eventDriverChannel, (Byte*) &evt, sizeof(evt));
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


        pChannel->count = KeyMap_Map(pChannel->map, &evt.data.key, pChannel->buffer, kKeyMap_MaxByteSequenceLength);

        Int i = 0;
        while (nBytesRead < nBytesToRead && pChannel->count > 0) {
            pBuffer[nBytesRead++] = pChannel->buffer[i++];
            pChannel->count--;
        }

        if (pChannel->count > 0) {
            // We ran out of space in the buffer that the user gave us. Remember
            // which bytes we need to copy next time read() is called.
            pChannel->startIndex = i;
            break;
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

        vtparser_byte(&pConsole->vtparser, by);
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
