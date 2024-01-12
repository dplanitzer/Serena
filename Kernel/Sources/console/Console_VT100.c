//
//  Console_VT100.c
//  Apollo
//
//  Created by Dietmar Planitzer on 1/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"


static Bool has_private_use_char(ConsoleRef _Nonnull pConsole, Character ch)
{
    return pConsole->vtparser.vt500.num_intermediate_chars > 0 && pConsole->vtparser.vt500.intermediate_chars[0] == ch;
}

static Int get_csi_parameter(ConsoleRef _Nonnull pConsole, Int defValue)
{
    const Int val = (pConsole->vtparser.vt500.num_params > 0) ? pConsole->vtparser.vt500.params[0] : 0;
    return (val > 0) ? val : defValue;
}

static Int get_nth_csi_parameter(ConsoleRef _Nonnull pConsole, Int idx, Int defValue)
{
    const Int val = (pConsole->vtparser.vt500.num_params > idx) ? pConsole->vtparser.vt500.params[idx] : 0;
    return (val > 0) ? val : defValue;
}


// Interprets the given byte as a C0/C1 control character and either executes it or ignores it.
// \param pConsole the console
// \param ch the character
static void Console_VT100_Execute_C0_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    switch (ch) {
        case 0x05:  // ENQ (Transmit answerback message)
            Console_PostReport_Locked(pConsole, "");
            // XXX allow the user to set an answerback message
            break;

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
        case 0x0b:  // VT (Vertical Tab)
        case 0x0c:  // FF (Form Feed)
            Console_Execute_LF_Locked(pConsole);
            break;

        case 0x0d:  // CR (Carriage Return)
            Console_MoveCursorTo_Locked(pConsole, 0, pConsole->y);
            break;
            
        case 0x7f:  // DEL (Delete)
            Console_Execute_DEL_Locked(pConsole);
            break;
#if 0            
        // XXX Not supported by VT100 (VT200 thing)
        case 0x84:  // IND (Index)
            Console_MoveCursor_Locked(pConsole, kCursorMovement_AutoScroll, 0, 1);
            break;

        case 0x85:  // NEL (Next Line)
            Console_MoveCursor_Locked(pConsole, kCursorMovement_AutoScroll, -pConsole->x, 1);
            break;

        case 0x88:  // HTS (Horizontal Tabulation Set)
            TabStops_InsertStop(&pConsole->hTabStops, pConsole->x);
            break;

        case 0x8d:  // RI (Reverse Line Feed)
            Console_MoveCursor_Locked(pConsole, kCursorMovement_AutoScroll, 0, -1);
            break;

        case 0x94:  // CCH (Cancel Character)
            Console_Execute_BS_Locked(pConsole);
#endif            
        default:
            // Ignore it
            break;
    }
}

static void Console_VT100_CSI_TBC_Locked(ConsoleRef _Nonnull pConsole, Int op)
{
    switch (op) {
        case 0:
            TabStops_RemoveStop(&pConsole->hTabStops, pConsole->x);
            break;

        case 3:
            TabStops_RemoveAllStops(&pConsole->hTabStops);
            break;

        default:
            // Ignore
            break;
    }
}

static void Console_VT100_CSI_h_Locked(ConsoleRef _Nonnull pConsole)
{
    const Bool isPrivateMode = has_private_use_char(pConsole, '?');

    for (Int i = 0; i < pConsole->vtparser.vt500.num_params; i++) {
        const Int p = get_nth_csi_parameter(pConsole, i, 0);

        if (!isPrivateMode) {
            switch (p) {
                case 4: // IRM
                    pConsole->flags.isInsertionMode = true;
                    break;

                default:
                    break;
            }
        }
        else {
            switch (p) {
                case 7: // DECAWM
                    pConsole->flags.isAutoWrapEnabled = true;
                    break;

                case 25:
                    Console_SetCursorVisible_Locked(pConsole, true);
                    break;

                default:
                    break;
            }
        }
    }
}

static void Console_VT100_CSI_l_Locked(ConsoleRef _Nonnull pConsole)
{
    const Bool isPrivateMode = has_private_use_char(pConsole, '?');

    for (Int i = 0; i < pConsole->vtparser.vt500.num_params; i++) {
        const Int p = get_nth_csi_parameter(pConsole, i, 0);

        if (!isPrivateMode) {
            switch (p) {
                case 4: // IRM
                    pConsole->flags.isInsertionMode = false;
                    break;

                default:
                    break;
            }
        }
        else {
            switch (p) {
                case 2: // VT52ANM
                    Console_SetCompatibilityMode(pConsole, kCompatibilityMode_VT52);
                    break;

                case 7: // DECAWM
                    pConsole->flags.isAutoWrapEnabled = false;
                    break;

                case 25:
                    Console_SetCursorVisible_Locked(pConsole, false);
                    break;

                default:
                    break;
            }
        }
    }
}

static void Console_VT100_CSI_n_Locked(ConsoleRef _Nonnull pConsole)
{
    const Bool isPrivateMode = has_private_use_char(pConsole, '?');

    for (Int i = 0; i < pConsole->vtparser.vt500.num_params; i++) {
        const Int p = get_nth_csi_parameter(pConsole, i, 0);

        if (!isPrivateMode) {
            switch (p) {
                case 5: // Request status of terminal
                    Console_PostReport_Locked(pConsole, "\033[0n");   // OK
                    break;

                case 6: { // Request cursor position
                    Character numbuf[11];
                    Character buf[MAX_MESSAGE_LENGTH + 1];
                    Character* ptr = &buf[0];
                    *ptr++ = '\033';
                    *ptr++ = '[';
                    if (pConsole-> x > 0 || pConsole->y > 0) {
                        const Character* lb = Int64_ToString(pConsole->y + 1, 10, 10, '\0', numbuf, 11);
                        while (*lb != '\0') {
                            *ptr++ = *lb++;
                        }
                        *ptr++ = ';';
                        const Character* cb = Int64_ToString(pConsole->x + 1, 10, 10, '\0', numbuf, 11);
                        while (*cb != '\0') {
                            *ptr++ = *cb++;
                        }
                    }
                    *ptr++ = 'R';
                    *ptr = '\0';
                    Console_PostReport_Locked(pConsole, buf);
                    break;
                }

                default:
                    break;
            }
        }
        else {
            switch (p) {
                case 15: // Request status of printer
                    Console_PostReport_Locked(pConsole, "\033[?13n");  // None
                    break;

                default:
                    break;
            }
        }
    }
}

static void Console_VT100_CSI_c_Locked(ConsoleRef _Nonnull pConsole)
{
    const Int p = get_nth_csi_parameter(pConsole, 0, 0);

    switch (p) {
        case 0:
            Console_PostReport_Locked(pConsole, "\033[?6c");   // VT100
            break;

        default:
            break;
    }
}

static void Console_VT100_CSI_y_Locked(ConsoleRef _Nonnull pConsole)
{
    if (get_nth_csi_parameter(pConsole, 0, 0) != 2) {
        return;
    }

    switch (get_nth_csi_parameter(pConsole, 1, 0)) {
        case 1:
        case 2:
        case 4:
        case 9:
        case 10:
        case 12:
        case 16:
        case 24:
            Console_PostReport_Locked(pConsole, "\033[0n");   // OK
            break;

        default:
            break;
    }
}

static void Console_VT100_CSI_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    switch (ch) {
        case 'c':   // DA
            Console_VT100_CSI_c_Locked(pConsole);
            break;

        case 'h':
            Console_VT100_CSI_h_Locked(pConsole);
            break;

        case 'l':
            Console_VT100_CSI_l_Locked(pConsole);
            break;

        case 'n':   // DSR
            Console_VT100_CSI_n_Locked(pConsole);
            break;

        case 'y':   // DECTST
            Console_VT100_CSI_y_Locked(pConsole);
            break;

        case 'A':   // CUU
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, 0, -get_csi_parameter(pConsole, 1));
            break;

        case 'B':   // CUD
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, 0, get_csi_parameter(pConsole, 1));
            break;

        case 'C':   // CUF
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, get_csi_parameter(pConsole, 1), 0);
            break;

        case 'D':   // CUB
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, -get_csi_parameter(pConsole, 1), 0);
            break;

        case 'H':   // CUP
            Console_MoveCursorTo_Locked(pConsole, 0, 0);
            break;

        case 'f':   // HVP
            Console_MoveCursorTo_Locked(pConsole, get_nth_csi_parameter(pConsole, 1, 1) - 1, get_nth_csi_parameter(pConsole, 0, 1) - 1);
            break;

        case 'g':   // TBC
            Console_VT100_CSI_TBC_Locked(pConsole, get_csi_parameter(pConsole, 0));
            break;

        case 'K':   // EL
            Console_ClearLine_Locked(pConsole, pConsole->y, (ClearLineMode) get_csi_parameter(pConsole, 0));
            break;

        case 'J':   // ED
            Console_ClearScreen_Locked(pConsole, (ClearScreenMode) get_csi_parameter(pConsole, 0));
            break;

        case 'P':   // DCH
            Console_Execute_DCH_Locked(pConsole, get_csi_parameter(pConsole, 1));
            break;

        case 'L':   // IL
            Console_Execute_IL_Locked(pConsole, get_csi_parameter(pConsole, 1));
            break;

        case 'M':   // DL
            Console_Execute_DL_Locked(pConsole, get_csi_parameter(pConsole, 1));
            break;

        case 'N':   // SS2
            // G2 character set is the same as G0 character set
            break;

        case 'O':   // SS3
             // G3 character set is the same as G0 character set
             break;

        default:
            // Ignore
            break;
    }
}

static void Console_VT100_ESC_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    switch (ch) {
        case 'D':   // ANSI: IND
            Console_MoveCursor_Locked(pConsole, kCursorMovement_AutoScroll, 0, 1);
            break;

        case 'M':   // ANSI: RI
            Console_MoveCursor_Locked(pConsole, kCursorMovement_AutoScroll, 0, -1);
            break;

        case 'E':   // ANSI: NEL
            Console_MoveCursor_Locked(pConsole, kCursorMovement_AutoScroll, -pConsole->x, 1);
            break;

        case '7':   // ANSI: DECSC
            Console_SaveCursorState_Locked(pConsole);
            break;

        case '8':   // ANSI: DECRC
            Console_RestoreCursorState_Locked(pConsole);
            break;

        case 'H':   // ANSI: HTS
            TabStops_InsertStop(&pConsole->hTabStops, pConsole->x);
            break;

        case 'Z':   // ANSI: DECID
            Console_PostReport_Locked(pConsole, "\033[?6c");  // VT100
            break;

        case 'c':   // ANSI: RIS
            Console_ResetState_Locked(pConsole);
            break;

        default:
            // Ignore
            break;
    }
}

void Console_VT100_ParseByte_Locked(ConsoleRef _Nonnull pConsole, vt500parse_action_t action, unsigned char b)
{
    switch (action) {
        case VT500PARSE_ACTION_CSI_DISPATCH:
            Console_VT100_CSI_Locked(pConsole, b);
            break;

        case VT500PARSE_ACTION_ESC_DISPATCH:
            Console_VT100_ESC_Locked(pConsole, b);
            break;

        case VT500PARSE_ACTION_EXECUTE:
            Console_VT100_Execute_C0_Locked(pConsole, b);
            break;

        case VT500PARSE_ACTION_PRINT:
            Console_PrintByte_Locked(pConsole, b);
            break;

        default:
            // Ignore
            break;
    }
}
