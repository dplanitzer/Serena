//
//  Console_VT52.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"


// Interprets the given byte as a C0 control character and either executes it or ignores it.
// \param self the console
// \param ch the character
static void Console_VT52_Execute_C0_Locked(ConsoleRef _Nonnull self, unsigned char ch)
{
    switch (ch) {
        case 0x07:  // BEL (Bell)
            Console_Execute_BEL_Locked(self);
            break;

        case 0x08:  // BS (Backspace)
            Console_Execute_BS_Locked(self);
            break;

        case 0x09:  // HT (Tab)
            Console_Execute_HT_Locked(self);
            break;

        case 0x0a:  // LF (Line Feed)
            Console_Execute_LF_Locked(self);
            break;

        case 0x0d:  // CR (Carriage Return)
            Console_MoveCursorTo_Locked(self, 0, self->y);
            break;
            
        default:    // Ignore
            break;
    }
}

static void Console_VT52_ESC_Atari_Locked(ConsoleRef _Nonnull self, unsigned char ch)
{
    switch (ch) {
    case 'E':   // VT52+Atari: Clear screen
        Console_MoveCursorTo_Locked(self, 0, 0);
        Console_ClearScreen_Locked(self, kClearScreenMode_Whole);
        break;

    case 'b':   // VT52+Atari: Set foreground color. Parameter byte is a character of which the lowest 4 bits are used to specify the color index
        Console_SetForegroundColor_Locked(self, Color_MakeIndex(self->vtparser.vt52.params[0] & 0x07)); // XXX limited to 3 bits for now since we oly support 8 colors
        break;

    case 'c':   // VT52+Atari: Set background color. Parameter byte is a character of which the lowest 4 bits are used to specify the color index
        Console_SetBackgroundColor_Locked(self, Color_MakeIndex(self->vtparser.vt52.params[0] & 0x07)); // XXX limited to 3 bits for now since we oly support 8 colors
        break;

    case 'd':   // VT52+Atari: Clear to start of screen
        Console_ClearScreen_Locked(self, kClearScreenMode_ToBeginning);
        break;

    case 'e':   // VT52+Atari: Show cursor
        self->flags.isTextCursorVisible = true;
        break;

    case 'f':   // VT52+Atari: Hide cursor
        self->flags.isTextCursorVisible = false;
        break;

    case 'j':   // VT52+Atari: Save cursor
        Console_SaveCursorState_Locked(self);
        break;

    case 'k':   // VT52+Atari: Restore cursor
        Console_RestoreCursorState_Locked(self);
        break;

    case 'l':   // VT52+Atari: Clear line and move cursor to the left margin
        Console_ClearLine_Locked(self, self->y, kClearLineMode_Whole);
        Console_MoveCursorTo_Locked(self, 0, self->y);
        break;

    case 'o':   // VT52+Atari: Clear to start of line
        Console_ClearLine_Locked(self, self->y, kClearLineMode_ToBeginning);
        break;

    case 'p':   // VT52+Atari: Reverse On
        self->characterRendition.isReverse = 1;
        break;

    case 'q':   // VT52+Atari: Reverse Off
        self->characterRendition.isReverse = 0;
        break;

    case 'v':   // VT52+Atari: Auto-wrap on
        self->flags.isAutoWrapEnabled = true;
        break;

    case 'w':   // VT52+Atari: Auto-wrap off
        self->flags.isAutoWrapEnabled = false;
        break;

    default:    // Ignore
        break;
    }
}

static void Console_VT52_ESC_Locked(ConsoleRef _Nonnull self, unsigned char ch)
{
    switch (ch) {
        case 'A':   // Cursor up
            Console_MoveCursor_Locked(self, kCursorMovement_Clamp, 0, -1);
            break;

        case 'B':   // Cursor down
            Console_MoveCursor_Locked(self, kCursorMovement_Clamp, 0, 1);
            break;

        case 'C':   // Cursor right
            Console_MoveCursor_Locked(self, kCursorMovement_Clamp, 1, 0);
            break;

        case 'D':   // Cursor left
            Console_MoveCursor_Locked(self, kCursorMovement_Clamp, -1, 0);
            break;

        case 'H':   // Cursor home
            Console_MoveCursorTo_Locked(self, 0, 0);
            break;

        case 'Y': { // Direct cursor address
            const int y = self->vtparser.vt52.params[0] - 040;
            const int x = self->vtparser.vt52.params[1] - 040;

            // Y and X are treated differently. See: <https://vt100.net/dec/ek-vt5x-op-001.pdf>
            if (y >= self->bounds.top && y <= self->bounds.bottom) {
                Console_MoveCursorTo_Locked(self, x, y);
            }
        }
            break;

        case 'I':   // Reverse linefeed 
            Console_MoveCursor_Locked(self, kCursorMovement_AutoScroll, 0, -1);
            break;

        case 'K':   // Erase to end of line
            Console_ClearLine_Locked(self, self->y, kClearLineMode_ToEnd);
            break;

        case 'J':   // Erase to end of screen
            Console_ClearScreen_Locked(self, kClearScreenMode_ToEnd);
            break;

        case 'Z':   // Identify terminal type
            Console_PostReport_Locked(self, "\033/K");  // VT52 without copier
            break;

        case '<':   // DECANM
            Console_SetCompatibilityMode_Locked(self, kCompatibilityMode_ANSI);
            break;
            
        case '?':   // Alternate keypad mode echo back
            // Next byte is the character to print. Just ignore the ESC ?
            break;

        default:
            if (self->compatibilityMode == kCompatibilityMode_VT52_AtariExtensions) {
                Console_VT52_ESC_Atari_Locked(self, ch);
            }
            break;
    }
}

void Console_VT52_ParseByte_Locked(ConsoleRef _Nonnull self, vt52parse_action_t action, unsigned char b)
{
    switch (action) {
        case VT52PARSE_ACTION_ESC_DISPATCH:
            Console_VT52_ESC_Locked(self, b);
            break;

        case VT52PARSE_ACTION_EXECUTE:
            Console_VT52_Execute_C0_Locked(self, b);
            break;

        case VT52PARSE_ACTION_PRINT:
            Console_PrintByte_Locked(self, b);
            break;

        default:    // Ignore
            break;
    }
}
