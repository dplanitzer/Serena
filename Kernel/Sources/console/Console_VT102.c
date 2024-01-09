//
//  Console_VT52.c
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
static void Console_VT102_Execute_C0_C1_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    switch (ch) {
        case 0x05:  // ENQ (Transmit answerback message)
            // XXX implement me
            break;

        case 0x07:  // BEL (Bell)
            Console_Execute_BEL_Locked(pConsole);
            break;

        case 0x08:  // BS (Backspace)
        case 0x94:  // CCH (Cancel Character)
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
            
        default:
            // Ignore it
            break;
    }
}

static void Console_VT102_CSI_TBC_Locked(ConsoleRef _Nonnull pConsole, Int op)
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

static void Console_VT102_CSI_h_Locked(ConsoleRef _Nonnull pConsole)
{
    const Bool isPrivateMode = has_private_use_char(pConsole, '?');

    for (Int i = 0; i < pConsole->vtparser.vt500.num_params; i++) {
        const Int p = get_nth_csi_parameter(pConsole, i, 0);

        if (!isPrivateMode) {
            switch (p) {
                case 4: // ANSI: IRM
                    pConsole->flags.isInsertionMode = true;
                    break;

                default:
                    break;
            }
        }
        else {
            switch (p) {
                case 7: // ANSI: DECAWM
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

static void Console_VT102_CSI_l_Locked(ConsoleRef _Nonnull pConsole)
{
    const Bool isPrivateMode = has_private_use_char(pConsole, '?');

    for (Int i = 0; i < pConsole->vtparser.vt500.num_params; i++) {
        const Int p = get_nth_csi_parameter(pConsole, i, 0);

        if (!isPrivateMode) {
            switch (p) {
                case 4: // ANSI: IRM
                    pConsole->flags.isInsertionMode = false;
                    break;

                default:
                    break;
            }
        }
        else {
            switch (p) {
                case 2: // ANSI: VT52ANM
                    Console_SetCompatibilityMode(pConsole, kCompatibilityMode_VT52);
                    break;

                case 7: // ANSI: DECAWM
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

static void Console_VT102_CSI_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
{
    switch (ch) {
        case 'h':
            Console_VT102_CSI_h_Locked(pConsole);
            break;

        case 'l':
            Console_VT102_CSI_l_Locked(pConsole);
            break;

        case 'A':   // ANSI: CUU
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, 0, -get_csi_parameter(pConsole, 1));
            break;

        case 'B':   // ANSI: CUD
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, 0, get_csi_parameter(pConsole, 1));
            break;

        case 'C':   // ANSI: CUF
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, get_csi_parameter(pConsole, 1), 0);
            break;

        case 'D':   // ANSI: CUB
            Console_MoveCursor_Locked(pConsole, kCursorMovement_Clamp, -get_csi_parameter(pConsole, 1), 0);
            break;

        case 'H':   // ANSI: CUP
            Console_MoveCursorTo_Locked(pConsole, 0, 0);
            break;

        case 'f':   // ANSI: HVP
            Console_MoveCursorTo_Locked(pConsole, get_nth_csi_parameter(pConsole, 1, 1) - 1, get_nth_csi_parameter(pConsole, 0, 1) - 1);
            break;

        case 'g':   // ANSI: TBC
            Console_VT102_CSI_TBC_Locked(pConsole, get_csi_parameter(pConsole, 0));
            break;

        case 'K':   // ANSI: EL
            Console_ClearLine_Locked(pConsole, pConsole->y, (ClearLineMode) get_csi_parameter(pConsole, 0));
            break;

        case 'J':   // ANSI: ED
            Console_ClearScreen_Locked(pConsole, (ClearScreenMode) get_csi_parameter(pConsole, 0));
            break;

        case 'P':   // ANSI: DCH
            Console_Execute_DCH_Locked(pConsole, get_csi_parameter(pConsole, 1));
            break;

        case 'L':   // ANSI: IL
            Console_Execute_IL_Locked(pConsole, get_csi_parameter(pConsole, 1));
            break;

        case 'M':   // ANSI: DL
            Console_Execute_DL_Locked(pConsole, get_csi_parameter(pConsole, 1));
            break;

        default:
            // Ignore
            break;
    }
}

static void Console_VT102_ESC_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch)
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

        case 'c':   // ANSI: RIS
            Console_ResetState_Locked(pConsole);
            break;

        default:
            // Ignore
            break;
    }
}

void Console_VT102_ParseByte_Locked(ConsoleRef _Nonnull pConsole, vt500parse_action_t action, unsigned char b)
{
    switch (action) {
        case VT500PARSE_ACTION_CSI_DISPATCH:
            Console_VT102_CSI_Locked(pConsole, b);
            break;

        case VT500PARSE_ACTION_ESC_DISPATCH:
            Console_VT102_ESC_Locked(pConsole, b);
            break;

        case VT500PARSE_ACTION_EXECUTE:
            Console_VT102_Execute_C0_C1_Locked(pConsole, b);
            break;

        case VT500PARSE_ACTION_PRINT:
            Console_PrintByte_Locked(pConsole, b);
            break;

        default:
            // Ignore
            break;
    }
}
