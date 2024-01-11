//
//  Console_VT52.c
//  Apollo
//
//  Created by Dietmar Planitzer on 1/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"


// Interprets the given byte as a C0/C1 control character and either executes it or ignores it.
// \param pConsole the console
// \param ch the character
static void Console_VT52_Execute_C0_C1_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
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
            Console_Execute_LF_Locked(pConsole);
            break;

        case 0x0d:  // CR (Carriage Return)
            Console_MoveCursorTo_Locked(pConsole, 0, pConsole->y);
            break;
            
        default:
            // Ignore it
            break;
    }
}

static void Console_VT52_ESC_Atari_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    switch (ch) {
    case 'E': // VT52+Atari: Clear screen
        Console_MoveCursorTo_Locked(pConsole, 0, 0);
        Console_ClearScreen_Locked(pConsole, kClearScreenMode_Whole);
        break;

    case 'd': // VT52+Atari: Clear to start of screen
        Console_ClearScreen_Locked(pConsole, kClearScreenMode_ToBeginning);
        break;

    case 'e': // VT52+Atari: Show cursor
        Console_SetCursorVisible_Locked(pConsole, true);
        break;

    case 'f': // VT52+Atari: Hide cursor
        Console_SetCursorVisible_Locked(pConsole, false);
        break;

    case 'j': // VT52+Atari: Save cursor
        Console_SaveCursorState_Locked(pConsole);
        break;

    case 'k': // VT52+Atari: Restore cursor
        Console_RestoreCursorState_Locked(pConsole);
        break;

    case 'l': // VT52+Atari: Clear line and move cursor to the left margin
        Console_ClearLine_Locked(pConsole, pConsole->y, kClearLineMode_Whole);
        Console_MoveCursorTo_Locked(pConsole, 0, pConsole->y);
        break;

    case 'o': // VT52+Atari: Clear to start of line
        Console_ClearLine_Locked(pConsole, pConsole->y, kClearLineMode_ToBeginning);
        break;

    case 'v': // VT52+Atari: Auto-wrap on
        pConsole->flags.isAutoWrapEnabled = true;
        break;

    case 'w': // VT52+Atari: Auto-wrap off
        pConsole->flags.isAutoWrapEnabled = false;
        break;

    default:
        // Ignore
        break;
    }
}

static void Console_VT52_ESC_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    switch (ch) {
        case 'A':   // VT52: Cursor up
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, 0, -1);
            break;

        case 'B':   // VT52: Cursor down
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, 0, 1);
            break;

        case 'C':   // VT52: Cursor right
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, 1, 0);
            break;

        case 'D':   // VT52: Cursor left
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, -1, 0);
            break;

        case 'H':   // VT52: Cursor home
            Console_MoveCursorTo_Locked(pConsole, 0, 0);
            break;

        case 'Y': { // VT52: Direct cursor address
            const Int y = pConsole->vtparser.vt52.params[0] - 040;
            const Int x = pConsole->vtparser.vt52.params[1] - 040;

            // Y and X are treated differently. See: <https://vt100.net/dec/ek-vt5x-op-001.pdf>
            if (y >= pConsole->bounds.top && y <= pConsole->bounds.bottom) {
                Console_MoveCursorTo_Locked(pConsole, x, y);
            }
        }
            break;

        case 'I':   // VT52: Reverse linefeed 
            Console_MoveCursor_Locked(pConsole, kCursorMovement_AutoScroll, 0, -1);
            break;

        case 'K':   // VT52: Erase to end of line
            Console_ClearLine_Locked(pConsole, pConsole->y, kClearLineMode_ToEnd);
            break;

        case 'J':   // VT52: Erase to end of screen
            Console_ClearScreen_Locked(pConsole, kClearScreenMode_ToEnd);
            break;

        case 'Z':   // VT52: Identify terminal type
            Console_PostReport_Locked(pConsole, "\033/K");  // VT52 without copier
            break;

        case '<':   // VT52: DECANM
            Console_SetCompatibilityMode(pConsole, kCompatibilityMode_ANSI);
            break;
            
        default:
            if (pConsole->compatibilityMode == kCompatibilityMode_VT52_AtariExtensions) {
                Console_VT52_ESC_Atari_Locked(pConsole, ch);
            }
            break;
    }
}

void Console_VT52_ParseByte_Locked(ConsoleRef _Nonnull pConsole, vt52parse_action_t action, unsigned char b)
{
    switch (action) {
        case VT52PARSE_ACTION_ESC_DISPATCH:
            Console_VT52_ESC_Locked(pConsole, b);
            break;

        case VT52PARSE_ACTION_EXECUTE:
            Console_VT52_Execute_C0_C1_Locked(pConsole, b);
            break;

        case VT52PARSE_ACTION_PRINT:
            Console_PrintByte_Locked(pConsole, b);
            break;

        default:
            // Ignore
            break;
    }
}
