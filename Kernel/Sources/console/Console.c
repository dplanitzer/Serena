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

const char* const kConsoleName = "console";


// Creates a new console object. This console will display its output on the
// provided graphics device.
// \param pEventDriver the event driver to provide keyboard input
// \param pGDevice the graphics device
// \return the console; NULL on failure
errno_t Console_Create(EventDriverRef _Nonnull pEventDriver, GraphicsDriverRef _Nonnull pGDevice, ConsoleRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ConsoleRef self;

    try(Driver_Create(Console, kDriverModel_Sync, 0, &self));
    
    Lock_Init(&self->lock);

    self->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    try(Driver_Open((DriverRef)pEventDriver, kOpen_Read, &self->eventDriverChannel));
    try(RingBuffer_Init(&self->reportsQueue, 4 * (MAX_MESSAGE_LENGTH + 1)));

    self->gdevice = Object_RetainAs(pGDevice, GraphicsDriver);
    self->keyMap = (const KeyMap*) gKeyMap_usa;

    self->lineHeight = GLYPH_HEIGHT;
    self->characterWidth = GLYPH_WIDTH;
    self->compatibilityMode = kCompatibilityMode_ANSI;


    // Initialize the ANSI escape sequence parser
    vtparser_init(&self->vtparser, (vt52parse_callback_t)Console_VT52_ParseByte_Locked, (vt500parse_callback_t)Console_VT100_ParseByte_Locked, self);


    // Initialize the video subsystem
    Console_InitVideoOutput(self);


    // Reset the console to the default configuration
    try(Console_ResetState_Locked(self, false));


    *pOutSelf = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

// Deallocates the console.
// \param self the console
void Console_deinit(ConsoleRef _Nonnull self)
{
    Console_SetCursorBlinkingEnabled_Locked(self, false);
    Console_DeinitVideoOutput(self);
    RingBuffer_Deinit(&self->reportsQueue);

    self->keyMap = NULL;
    TabStops_Deinit(&self->hTabStops);
        
    Lock_Deinit(&self->lock);

    Object_Release(self->gdevice);
    self->gdevice = NULL;

    if (self->eventDriverChannel) {
        Driver_Close((DriverRef)self->eventDriver, self->eventDriverChannel);
        IOChannel_Release(self->eventDriverChannel);
        self->eventDriverChannel = NULL;
    }
    Object_Release(self->eventDriver);
    self->eventDriver = NULL;
}

static errno_t Console_start(ConsoleRef _Nonnull self)
{
    // Clear the console screen
    Console_BeginDrawing_Locked(self);
    Console_ClearScreen_Locked(self, kClearScreenMode_WholeAndScrollback);
    Console_EndDrawing_Locked(self);


    // Start cursor blinking
    Console_SetCursorBlinkingEnabled_Locked(self, true);

    return Driver_Publish((DriverRef)self, kConsoleName);
}

errno_t Console_ResetState_Locked(ConsoleRef _Nonnull self, bool shouldStartCursorBlinking)
{
    decl_try_err();
    
    self->bounds = Rect_Make(0, 0, self->gc.pixelsWidth / self->characterWidth, self->gc.pixelsHeight / self->lineHeight);
    self->savedCursorState.x = 0;
    self->savedCursorState.y = 0;

    Console_ResetCharacterAttributes_Locked(self);

    TabStops_Deinit(&self->hTabStops);
    try(TabStops_Init(&self->hTabStops, __max(Rect_GetWidth(self->bounds) / 8, 0), 8));

    Console_MoveCursorTo_Locked(self, 0, 0);
    Console_SetCursorVisible_Locked(self, true);
    Console_SetCursorBlinkingEnabled_Locked(self, shouldStartCursorBlinking);
    self->flags.isAutoWrapEnabled = true;
    self->flags.isInsertionMode = false;

    return EOK;

catch:
    return err;
}

void Console_ResetCharacterAttributes_Locked(ConsoleRef _Nonnull self)
{
    Console_SetDefaultForegroundColor_Locked(self);
    Console_SetDefaultBackgroundColor_Locked(self);
    self->characterRendition.isBold = 0;
    self->characterRendition.isDimmed = 0;
    self->characterRendition.isItalic = 0;
    self->characterRendition.isUnderlined = 0;
    self->characterRendition.isBlink = 0;
    self->characterRendition.isReverse = 0;
    self->characterRendition.isHidden = 0;
    self->characterRendition.isStrikethrough = 0;
}

// Switches the console to the given compatibility mode
void Console_SetCompatibilityMode_Locked(ConsoleRef _Nonnull self, CompatibilityMode mode)
{
    vtparser_mode_t vtmode;

    switch (mode) {
        case kCompatibilityMode_VT52:
            vtmode = VTPARSER_MODE_VT52;
            self->flags.isInsertionMode = false;
            self->flags.isAutoWrapEnabled = false;
            break;

        case kCompatibilityMode_VT52_AtariExtensions:
            vtmode = VTPARSER_MODE_VT52_ATARI;
            self->flags.isInsertionMode = false;
            self->flags.isAutoWrapEnabled = false;
            break;

        case kCompatibilityMode_VT100:
            vtmode = VTPARSER_MODE_VT100;
            break;

        default:
            abort();
            break;
    }

    vtparser_set_mode(&self->vtparser, vtmode);
    self->compatibilityMode = mode;
}

// Scrolls the content of the console screen. 'clipRect' defines a viewport
// through which a virtual document is visible. This viewport is scrolled by
// 'dX' / 'dY' character cells. Positive values move the viewport down/right
// (and scroll the virtual document up/left) and negative values move the
// viewport up/left (and scroll the virtual document down/right).
static void Console_ScrollBy_Locked(ConsoleRef _Nonnull self, int dX, int dY)
{
    const Rect clipRect = self->bounds;
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

            Console_CopyRect_Locked(self, copyRect, dstLoc);
        }


        if (absDy > 0) {
            Rect vClearRect;

            vClearRect.left = clipRect.left;
            vClearRect.top = (dY < 0) ? clipRect.top : clipRect.bottom - absDy;
            vClearRect.right = clipRect.right;
            vClearRect.bottom = (dY < 0) ? clipRect.top + absDy : clipRect.bottom;
            Console_FillRect_Locked(self, vClearRect, ' ');
        }


        if (absDx > 0) {
            Rect hClearRect;

            hClearRect.left = (dX < 0) ? clipRect.left : clipRect.right - absDx;
            hClearRect.top = (dY < 0) ? clipRect.top + absDy : clipRect.top;
            hClearRect.right = (dX < 0) ? clipRect.left + absDx : clipRect.right;
            hClearRect.bottom = (dY < 0) ? clipRect.bottom : clipRect.bottom - absDy;

            Console_FillRect_Locked(self, hClearRect, ' ');
        }
    }
    else {
        Console_ClearScreen_Locked(self, kClearScreenMode_Whole);
    }
}

// Clears the console screen.
// \param self the console
// \param mode the clear screen mode
void Console_ClearScreen_Locked(ConsoleRef _Nonnull self, ClearScreenMode mode)
{
    switch (mode) {
        case kClearScreenMode_ToEnd:
            Console_FillRect_Locked(self, Rect_Make(self->x, self->y, self->bounds.right, self->y + 1), ' ');
            Console_FillRect_Locked(self, Rect_Make(0, self->y + 1, self->bounds.right, self->bounds.bottom), ' ');
            break;

        case kClearScreenMode_ToBeginning:
            Console_FillRect_Locked(self, Rect_Make(0, self->y, self->x, self->y + 1), ' ');
            Console_FillRect_Locked(self, Rect_Make(0, 0, self->bounds.right, self->y - 1), ' ');
            break;

        case kClearScreenMode_Whole:
        case kClearScreenMode_WholeAndScrollback:
            Console_FillRect_Locked(self, self->bounds, ' ');
            break;

        default:
            // Ignore
            break;
    }
}

// Clears the specified line. Does not change the cursor position.
void Console_ClearLine_Locked(ConsoleRef _Nonnull self, int y, ClearLineMode mode)
{
    if (Rect_Contains(self->bounds, 0, y)) {
        int left, right;

        switch (mode) {
            case kClearLineMode_ToEnd:
                left = self->x;
                right = self->bounds.right;
                break;

            case kClearLineMode_ToBeginning:
                left = 0;
                right = self->x;
                break;

            case kClearLineMode_Whole:
                left = 0;
                right = self->bounds.right;
                break;

            default:
                // Ignore
                return;
        }

        Console_FillRect_Locked(self, Rect_Make(left, y, right, y + 1), ' ');
    }
}

void Console_SaveCursorState_Locked(ConsoleRef _Nonnull self)
{
    self->savedCursorState.x = self->x;
    self->savedCursorState.y = self->y;
    self->savedCursorState.foregroundColor = self->foregroundColor;
    self->savedCursorState.backgroundColor = self->backgroundColor;
    self->savedCursorState.characterRendition = self->characterRendition;
}

void Console_RestoreCursorState_Locked(ConsoleRef _Nonnull self)
{
    self->characterRendition = self->savedCursorState.characterRendition;
    Console_SetForegroundColor_Locked(self, self->savedCursorState.foregroundColor);
    Console_SetBackgroundColor_Locked(self, self->savedCursorState.backgroundColor);
    Console_MoveCursorTo_Locked(self, self->savedCursorState.x, self->savedCursorState.y);
}

// Moves the console position by the given delta values.
// \param self the console
// \param mode how the cursor movement should be handled if it tries to go past the margins
// \param dx the X delta
// \param dy the Y delta
void Console_MoveCursor_Locked(ConsoleRef _Nonnull self, CursorMovement mode, int dx, int dy)
{
    if (dx == 0 && dy == 0) {
        return;
    }
    
    const int aX = 0;
    const int aY = 0;
    const int eX = self->bounds.right - 1;
    const int eY = self->bounds.bottom - 1;
    int x = self->x + dx;
    int y = self->y + dy;

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
                Console_ScrollBy_Locked(self, 0, y);
                y = aY;
            }
            else if (y > eY) {
                Console_ScrollBy_Locked(self, 0, y - eY);
                y = eY;
            }
            break;

        case kCursorMovement_AutoScroll:
            x = __max(__min(x, eX), aX);

            if (y < aY) {
                Console_ScrollBy_Locked(self, 0, y);
                y = aY;
            }
            else if (y > eY) {
                Console_ScrollBy_Locked(self, 0, y - eY);
                y = eY;
            }

            break;

        default:
            abort();
            break;
    }

    self->x = x;
    self->y = y;
    Console_CursorDidMove_Locked(self);
}

// Sets the console position. The next print() will start printing at this
// location.
// \param self the console
// \param x the X position
// \param y the Y position
void Console_MoveCursorTo_Locked(ConsoleRef _Nonnull self, int x, int y)
{
    Console_MoveCursor_Locked(self, kCursorMovement_Clamp, x - self->x, y - self->y);
}


////////////////////////////////////////////////////////////////////////////////
// Terminal reports queue
////////////////////////////////////////////////////////////////////////////////

// Posts a terminal report to the reports queue. The message length may not
// exceed MAX_MESSAGE_LENGTH.
void Console_PostReport_Locked(ConsoleRef _Nonnull self, const char* msg)
{
    const ssize_t nBytesToWrite = String_Length(msg) + 1;
    assert(nBytesToWrite < (MAX_MESSAGE_LENGTH + 1));

    // Make space for the new message by removing the oldest (full) message(s)
    while (RingBuffer_WritableCount(&self->reportsQueue) < nBytesToWrite) {
        char b;

        // Remove a full message
        do {
            RingBuffer_GetByte(&self->reportsQueue, &b);
        } while (b != 0);
    }

    // Queue the new terminal report (including the trailing \0)
    RingBuffer_PutBytes(&self->reportsQueue, msg, nBytesToWrite);
}


////////////////////////////////////////////////////////////////////////////////
// Processing input bytes
////////////////////////////////////////////////////////////////////////////////

// Interprets the given byte as a character, maps it to a glyph and prints it.
// \param self the console
// \param ch the character
void Console_PrintByte_Locked(ConsoleRef _Nonnull self, unsigned char ch)
{
    // The cursor position is always valid and inside the framebuffer
    if (self->flags.isInsertionMode) {
        Console_CopyRect_Locked(self, Rect_Make(self->x, self->y, self->bounds.right - 1, self->y + 1), Point_Make(self->x + 1, self->y));
    }

    Console_DrawGlyph_Locked(self, ch, self->x, self->y);
    Console_MoveCursor_Locked(self, (self->flags.isAutoWrapEnabled) ? kCursorMovement_AutoWrap : kCursorMovement_Clamp, 1, 0);
}

void Console_Execute_BEL_Locked(ConsoleRef _Nonnull self)
{
    // XXX implement me
    // XXX flash screen?
}

void Console_Execute_HT_Locked(ConsoleRef _Nonnull self)
{
    Console_MoveCursorTo_Locked(self, TabStops_GetNextStop(&self->hTabStops, self->x, Rect_GetWidth(self->bounds)), self->y);
}

// Line feed may be IND or NEL depending on a setting (that doesn't exist yet)
void Console_Execute_LF_Locked(ConsoleRef _Nonnull self)
{
    Console_MoveCursor_Locked(self, kCursorMovement_AutoScroll, -self->x, 1);
}

void Console_Execute_BS_Locked(ConsoleRef _Nonnull self)
{
    if (self->x > 0) {
        // BS moves 1 cell to the left. It moves all characters in the line and
        // to the right of the cursor one column to the left if insertion mode
        // is enabled.
        if (self->flags.isInsertionMode) {
            Console_CopyRect_Locked(self, Rect_Make(self->x, self->y, self->bounds.right, self->y + 1), Point_Make(self->x - 1, self->y));
            Console_FillRect_Locked(self, Rect_Make(self->bounds.right - 1, self->y, self->bounds.right, self->y + 1), ' ');
        }
        Console_MoveCursor_Locked(self, kCursorMovement_Clamp, -1, 0);
    }
}

void Console_Execute_DEL_Locked(ConsoleRef _Nonnull self)
{
    if (self->x < self->bounds.right - 1) {
        // DEL does not change the position.
        Console_CopyRect_Locked(self, Rect_Make(self->x + 1, self->y, self->bounds.right, self->y + 1), Point_Make(self->x, self->y));
        Console_FillRect_Locked(self, Rect_Make(self->bounds.right - 1, self->y, self->bounds.right, self->y + 1), ' ');
    }
}

void Console_Execute_DCH_Locked(ConsoleRef _Nonnull self, int nChars)
{
    Console_CopyRect_Locked(self, Rect_Make(self->x + nChars, self->y, self->bounds.right - nChars, self->y + 1), Point_Make(self->x, self->y));
    Console_FillRect_Locked(self, Rect_Make(self->bounds.right - nChars, self->y, self->bounds.right, self->y + 1), ' ');
}

void Console_Execute_IL_Locked(ConsoleRef _Nonnull self, int nLines)
{
    if (self->y < self->bounds.bottom) {
        Console_CopyRect_Locked(self, Rect_Make(0, self->y + 1, self->bounds.right, self->bounds.bottom - nLines), Point_Make(self->x, self->y + 2));
        Console_FillRect_Locked(self, Rect_Make(0, self->y + 1, self->bounds.right, self->y + 1 + nLines), ' ');
    }
}

void Console_Execute_DL_Locked(ConsoleRef _Nonnull self, int nLines)
{
    Console_CopyRect_Locked(self, Rect_Make(0, self->y + 1, self->bounds.right, self->bounds.bottom - nLines), Point_Make(self->x, self->y));
    Console_FillRect_Locked(self, Rect_Make(0, self->bounds.bottom - nLines, self->bounds.right, self->bounds.bottom), ' ');
}


errno_t Console_createChannel(ConsoleRef _Nonnull self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return ConsoleChannel_Create(self, mode, pOutChannel);
}

static void Console_ReadReports_NonBlocking_Locked(ConsoleRef _Nonnull self, ConsoleChannelRef _Nonnull pChannel, char* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    ssize_t nBytesRead = 0;

    while (nBytesRead < nBytesToRead) {
        bool done = false;

        while (true) {
            char b;

            if (RingBuffer_GetByte(&self->reportsQueue, &b) == 0) {
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

static errno_t Console_ReadEvents_Locked(ConsoleRef _Nonnull self, ConsoleChannelRef _Nonnull pChannel, char* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
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
        Lock_Unlock(&self->lock);
        // XXX Need an API that allows me to read as many events as possible without blocking and that only blocks if there are no events available
        // XXX Or, probably, that's how the event driver read() should work in general
        const errno_t e1 = IOChannel_Read(self->eventDriverChannel, &evt, sizeof(evt), &nEvtBytesRead);
        Lock_Lock(&self->lock);
        // XXX we are currently assuming here that no relevant console state has
        // XXX changed while we didn't hold the lock. Confirm that this is okay
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }

        if (evt.type != kHIDEventType_KeyDown) {
            continue;
        }


        pChannel->rdCount = KeyMap_Map(self->keyMap, &evt.data.key, pChannel->rdBuffer, MAX_MESSAGE_LENGTH);

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
errno_t Console_read(ConsoleRef _Nonnull self, ConsoleChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
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
// \param self the console
// \param pBytes the byte sequence
// \param nBytes the number of bytes to write
// \return the number of bytes written; a negative error code if an error was encountered
errno_t Console_write(ConsoleRef _Nonnull self, ConsoleChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    const unsigned char* pChars = pBuffer;
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
    *nOutBytesWritten = (err == EOK) ? nBytesToWrite : 0;
    Lock_Unlock(&self->lock);

    return err;
}


class_func_defs(Console, Driver,
override_func_def(deinit, Console, Object)
override_func_def(start, Console, Driver)
override_func_def(createChannel, Console, Driver)
override_func_def(read, Console, Driver)
override_func_def(write, Console, Driver)
);
