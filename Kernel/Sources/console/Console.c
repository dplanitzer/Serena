//
//  Console.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"
#include "ConsoleChannel.h"
#include <driver/hid/EventChannel.h>


// Creates a new console object. This console will display its output on the
// provided graphics device.
// \param pEventDriver the event driver to provide keyboard input
// \param pGDevice the graphics device
// \return the console; NULL on failure
errno_t Console_Create(EventDriverRef _Nonnull pEventDriver, GraphicsDriverRef _Nonnull pGDevice, ConsoleRef _Nullable * _Nonnull pOutConsole)
{
    decl_try_err();
    ConsoleRef pConsole;

    try(Object_Create(Console, &pConsole));
    
    Lock_Init(&pConsole->lock);

    pConsole->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    try(EventChannel_Create((ObjectRef)pConsole->eventDriver, kOpen_Read, &pConsole->eventDriverChannel));
    try(RingBuffer_Init(&pConsole->reportsQueue, 4 * (MAX_MESSAGE_LENGTH + 1)));

    pConsole->gdevice = Object_RetainAs(pGDevice, GraphicsDriver);
    pConsole->keyMap = (const KeyMap*) gKeyMap_usa;

    pConsole->lineHeight = GLYPH_HEIGHT;
    pConsole->characterWidth = GLYPH_WIDTH;
    pConsole->compatibilityMode = kCompatibilityMode_ANSI;


    // Initialize the ANSI escape sequence parser
    vtparser_init(&pConsole->vtparser, (vt52parse_callback_t)Console_VT52_ParseByte_Locked, (vt500parse_callback_t)Console_VT100_ParseByte_Locked, pConsole);


    // Initialize the video subsystem
    Console_InitVideoOutput(pConsole);


    // Reset the console to the default configuration
    try(Console_ResetState_Locked(pConsole));


    // Clear the console screen
    Console_BeginDrawing_Locked(pConsole);
    Console_ClearScreen_Locked(pConsole, kClearScreenMode_WhileAndScrollback);
    Console_EndDrawing_Locked(pConsole);

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
    Console_DeinitVideoOutput(pConsole);
    RingBuffer_Deinit(&pConsole->reportsQueue);

    pConsole->keyMap = NULL;
    TabStops_Deinit(&pConsole->hTabStops);
        
    Lock_Deinit(&pConsole->lock);

    Object_Release(pConsole->gdevice);
    pConsole->gdevice = NULL;

    if (pConsole->eventDriverChannel) {
        IOChannel_Release(pConsole->eventDriverChannel);
        pConsole->eventDriverChannel = NULL;
    }
    Object_Release(pConsole->eventDriver);
    pConsole->eventDriver = NULL;
}

errno_t Console_ResetState_Locked(ConsoleRef _Nonnull pConsole)
{
    decl_try_err();
    
    pConsole->bounds = Rect_Make(0, 0, pConsole->gc.pixelsWidth / pConsole->characterWidth, pConsole->gc.pixelsHeight / pConsole->lineHeight);
    pConsole->savedCursorState.x = 0;
    pConsole->savedCursorState.y = 0;

    Console_ResetCharacterAttributes_Locked(pConsole);

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

void Console_ResetCharacterAttributes_Locked(ConsoleRef _Nonnull pConsole)
{
    Console_SetDefaultForegroundColor_Locked(pConsole);
    Console_SetDefaultBackgroundColor_Locked(pConsole);
    pConsole->characterRendition.isBold = 0;
    pConsole->characterRendition.isDimmed = 0;
    pConsole->characterRendition.isItalic = 0;
    pConsole->characterRendition.isUnderlined = 0;
    pConsole->characterRendition.isBlink = 0;
    pConsole->characterRendition.isReverse = 0;
    pConsole->characterRendition.isHidden = 0;
    pConsole->characterRendition.isStrikethrough = 0;
}

// Switches the console to the given compatibility mode
void Console_SetCompatibilityMode_Locked(ConsoleRef _Nonnull pConsole, CompatibilityMode mode)
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

        case kCompatibilityMode_VT100:
            vtmode = VTPARSER_MODE_VT100;
            break;

        default:
            abort();
            break;
    }

    vtparser_set_mode(&pConsole->vtparser, vtmode);
    pConsole->compatibilityMode = mode;
}

// Scrolls the content of the console screen. 'clipRect' defines a viewport
// through which a virtual document is visible. This viewport is scrolled by
// 'dX' / 'dY' character cells. Positive values move the viewport down/right
// (and scroll the virtual document up/left) and negative values move the
// viewport up/left (and scroll the virtual document down/right).
static void Console_ScrollBy_Locked(ConsoleRef _Nonnull pConsole, int dX, int dY)
{
    const Rect clipRect = pConsole->bounds;
    const int absDx = __abs(dX), absDy = __abs(dY);

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
            Console_FillRect_Locked(pConsole, pConsole->bounds, ' ');
            break;

        default:
            // Ignore
            break;
    }
}

// Clears the specified line. Does not change the cursor position.
void Console_ClearLine_Locked(ConsoleRef _Nonnull pConsole, int y, ClearLineMode mode)
{
    if (Rect_Contains(pConsole->bounds, 0, y)) {
        int left, right;

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
    pConsole->savedCursorState.foregroundColor = pConsole->foregroundColor;
    pConsole->savedCursorState.backgroundColor = pConsole->backgroundColor;
    pConsole->savedCursorState.characterRendition = pConsole->characterRendition;
}

void Console_RestoreCursorState_Locked(ConsoleRef _Nonnull pConsole)
{
    pConsole->characterRendition = pConsole->savedCursorState.characterRendition;
    Console_SetForegroundColor_Locked(pConsole, pConsole->savedCursorState.foregroundColor);
    Console_SetBackgroundColor_Locked(pConsole, pConsole->savedCursorState.backgroundColor);
    Console_MoveCursorTo_Locked(pConsole, pConsole->savedCursorState.x, pConsole->savedCursorState.y);
}

// Moves the console position by the given delta values.
// \param pConsole the console
// \param mode how the cursor movement should be handled if it tries to go past the margins
// \param dx the X delta
// \param dy the Y delta
void Console_MoveCursor_Locked(ConsoleRef _Nonnull pConsole, CursorMovement mode, int dx, int dy)
{
    if (dx == 0 && dy == 0) {
        return;
    }
    
    const int aX = 0;
    const int aY = 0;
    const int eX = pConsole->bounds.right - 1;
    const int eY = pConsole->bounds.bottom - 1;
    int x = pConsole->x + dx;
    int y = pConsole->y + dy;

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
void Console_MoveCursorTo_Locked(ConsoleRef _Nonnull pConsole, int x, int y)
{
    Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, x - pConsole->x, y - pConsole->y);
}


////////////////////////////////////////////////////////////////////////////////
// Terminal reports queue
////////////////////////////////////////////////////////////////////////////////

// Posts a terminal report to the reports queue. The message length may not
// exceed MAX_MESSAGE_LENGTH.
void Console_PostReport_Locked(ConsoleRef _Nonnull pConsole, const char* msg)
{
    const ssize_t nBytesToWrite = String_Length(msg) + 1;
    assert(nBytesToWrite < (MAX_MESSAGE_LENGTH + 1));

    // Make space for the new message by removing the oldest (full) message(s)
    while (RingBuffer_WritableCount(&pConsole->reportsQueue) < nBytesToWrite) {
        char b;

        // Remove a full message
        do {
            RingBuffer_GetByte(&pConsole->reportsQueue, &b);
        } while (b != 0);
    }

    // Queue the new terminal report (including the trailing \0)
    RingBuffer_PutBytes(&pConsole->reportsQueue, msg, nBytesToWrite);
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

    Console_DrawGlyph_Locked(pConsole, ch, pConsole->x, pConsole->y);
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

void Console_Execute_DCH_Locked(ConsoleRef _Nonnull pConsole, int nChars)
{
    Console_CopyRect_Locked(pConsole, Rect_Make(pConsole->x + nChars, pConsole->y, pConsole->bounds.right - nChars, pConsole->y + 1), Point_Make(pConsole->x, pConsole->y));
    Console_FillRect_Locked(pConsole, Rect_Make(pConsole->bounds.right - nChars, pConsole->y, pConsole->bounds.right, pConsole->y + 1), ' ');
}

void Console_Execute_IL_Locked(ConsoleRef _Nonnull pConsole, int nLines)
{
    if (pConsole->y < pConsole->bounds.bottom) {
        Console_CopyRect_Locked(pConsole, Rect_Make(0, pConsole->y + 1, pConsole->bounds.right, pConsole->bounds.bottom - nLines), Point_Make(pConsole->x, pConsole->y + 2));
        Console_FillRect_Locked(pConsole, Rect_Make(0, pConsole->y + 1, pConsole->bounds.right, pConsole->y + 1 + nLines), ' ');
    }
}

void Console_Execute_DL_Locked(ConsoleRef _Nonnull pConsole, int nLines)
{
    Console_CopyRect_Locked(pConsole, Rect_Make(0, pConsole->y + 1, pConsole->bounds.right, pConsole->bounds.bottom - nLines), Point_Make(pConsole->x, pConsole->y));
    Console_FillRect_Locked(pConsole, Rect_Make(0, pConsole->bounds.bottom - nLines, pConsole->bounds.right, pConsole->bounds.bottom), ' ');
}

static void Console_ReadReports_NonBlocking_Locked(ConsoleRef _Nonnull pConsole, ConsoleChannelRef _Nonnull pChannel, char* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    ssize_t nBytesRead = 0;

    while (nBytesRead < nBytesToRead) {
        bool done = false;

        while (true) {
            char b;

            if (RingBuffer_GetByte(&pConsole->reportsQueue, &b) == 0) {
                done = true;
                break;
            }
            if (b == 0) {
                break;
            }

            pChannel->rdBuffer[pChannel->rdCount++] = b;
        }
        if (done) {
            break;
        }

        int i = 0;
        while (nBytesRead < nBytesToRead && pChannel->rdCount > 0) {
            pBuffer[nBytesRead++] = pChannel->rdBuffer[i++];
            pChannel->rdCount--;
        }

        if (pChannel->rdCount > 0) {
            // We ran out of space in the buffer that the user gave us. Remember
            // which bytes we need to copy next time read() is called.
            pChannel->rdIndex = i;
            break;
        }
    }
    
    *nOutBytesRead = nBytesRead;
}

static errno_t Console_ReadEvents_Locked(ConsoleRef _Nonnull pConsole, ConsoleChannelRef _Nonnull pChannel, char* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    HIDEvent evt;
    ssize_t nBytesRead = 0;
    ssize_t nEvtBytesRead;

    while (nBytesRead < nBytesToRead) {
        // Drop the console lock while getting an event since the get events call
        // may block and holding the lock while being blocked for a potentially
        // long time would prevent any other process from working with the
        // console
        Lock_Unlock(&pConsole->lock);
        // XXX Need an API that allows me to read as many events as possible without blocking and that only blocks if there are no events available
        // XXX Or, probably, that's how the event driver read() should work in general
        const errno_t e1 = IOChannel_Read(pConsole->eventDriverChannel, &evt, sizeof(evt), &nEvtBytesRead);
        Lock_Lock(&pConsole->lock);
        // XXX we are currently assuming here that no relevant console state has
        // XXX changed while we didn't hold the lock. Confirm that this is okay
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }

        if (evt.type != kHIDEventType_KeyDown) {
            continue;
        }


        pChannel->rdCount = KeyMap_Map(pConsole->keyMap, &evt.data.key, pChannel->rdBuffer, MAX_MESSAGE_LENGTH);

        int i = 0;
        while (nBytesRead < nBytesToRead && pChannel->rdCount > 0) {
            pBuffer[nBytesRead++] = pChannel->rdBuffer[i++];
            pChannel->rdCount--;
        }

        if (pChannel->rdCount > 0) {
            // We ran out of space in the buffer that the user gave us. Remember
            // which bytes we need to copy next time read() is called.
            pChannel->rdIndex = i;
            break;
        }
    }
    
    *nOutBytesRead = nBytesRead;
    return err;
}

// Note that this read implementation will only block if there is no buffered
// data, no terminal reports and no events are available. It tries to do a
// non-blocking read as hard as possible even if it can't fully fill the user
// provided buffer. 
errno_t Console_Read(ConsoleRef _Nonnull self, ConsoleChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    char* pChars = pBuffer;
    HIDEvent evt;
    int evtCount;
    ssize_t nBytesRead = 0;
    ssize_t nTmpBytesRead;

    Lock_Lock(&self->lock);

    // First check whether we got a partial key byte sequence sitting in our key
    // mapping buffer and copy that one out.
    while (nBytesRead < nBytesToRead && pChannel->rdCount > 0) {
        pChars[nBytesRead++] = pChannel->rdBuffer[pChannel->rdIndex++];
        pChannel->rdCount--;
    }


    if (!RingBuffer_IsEmpty(&self->reportsQueue)) {
        // Now check whether there are terminal reports pending. Those take
        // priority over input device events.
        Console_ReadReports_NonBlocking_Locked(self, pChannel, &pChars[nBytesRead], nBytesToRead - nBytesRead, &nTmpBytesRead);
        nBytesRead += nTmpBytesRead;
    }


    if (nBytesRead == 0 && err == EOK) {
        // We haven't read any data so far. Read input events and block if none
        // are available either.
        const errno_t e1 = Console_ReadEvents_Locked(self, pChannel, &pChars[nBytesRead], nBytesToRead - nBytesRead, &nTmpBytesRead);
        if (e1 == EOK) {
            nBytesRead += nTmpBytesRead;
        } else {
            err = (nBytesRead == 0) ? e1 : EOK;
        }
    }

    Lock_Unlock(&self->lock);

    *nOutBytesRead = nBytesRead;
    return err;
}

// Writes the given byte sequence of characters to the console.
// \param pConsole the console
// \param pBytes the byte sequence
// \param nBytes the number of bytes to write
// \return the number of bytes written; a negative error code if an error was encountered
errno_t Console_Write(ConsoleRef _Nonnull self, const void* _Nonnull pBytes, ssize_t nBytesToWrite)
{
    decl_try_err();
    const unsigned char* pChars = pBytes;
    const unsigned char* pCharsEnd = pChars + nBytesToWrite;

    Lock_Lock(&self->lock);
    err = Console_BeginDrawing_Locked(self);
    if (err == EOK) {
        while (pChars < pCharsEnd) {
            const unsigned char by = *pChars++;

            vtparser_byte(&self->vtparser, by);
        }

        Console_EndDrawing_Locked(self);
    }
    Lock_Unlock(&self->lock);
    return err;
}


class_func_defs(Console, Driver,
override_func_def(deinit, Console, Object)
);
