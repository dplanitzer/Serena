//
//  Console_VT100.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"
#include "ConsoleChannel.h"


static bool has_private_use_char(ConsoleRef _Nonnull self, char ch)
{
    return self->vtparser.vt500.num_intermediate_chars > 0 && self->vtparser.vt500.intermediate_chars[0] == ch;
}

static int get_csi_parameter(ConsoleRef _Nonnull self, int defValue)
{
    const int val = (self->vtparser.vt500.num_params > 0) ? self->vtparser.vt500.params[0] : 0;
    return (val > 0) ? val : defValue;
}

static int get_nth_csi_parameter(ConsoleRef _Nonnull self, int idx, int defValue)
{
    const int val = (self->vtparser.vt500.num_params > idx) ? self->vtparser.vt500.params[idx] : 0;
    return (val > 0) ? val : defValue;
}


// Interprets the given byte as a C0/C1 control character and either executes it or ignores it.
// \param self the console
// \param ch the character
static void Console_VT100_Execute_C0_Locked(ConsoleRef _Nonnull self, unsigned char ch)
{
    switch (ch) {
        case 0x05:  // ENQ (Transmit answerback message)
            Console_PostReport_Locked(self, "");
            // XXX allow the user to set an answerback message
            break;

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
        case 0x0b:  // VT (Vertical Tab)
        case 0x0c:  // FF (Form Feed)
            Console_Execute_LF_Locked(self);
            break;

        case 0x0d:  // CR (Carriage Return)
            Console_MoveCursorTo_Locked(self, 0, self->y);
            break;
            
        case 0x7f:  // DEL (Delete)
            Console_Execute_DEL_Locked(self);
            break;
#if 0            
        // XXX Not supported by VT100 (VT200 thing)
        case 0x84:  // IND (Index)
            Console_MoveCursor_Locked(self, kCursorMovement_AutoScroll, 0, 1);
            break;

        case 0x85:  // NEL (Next Line)
            Console_MoveCursor_Locked(self, kCursorMovement_AutoScroll, -self->x, 1);
            break;

        case 0x88:  // HTS (Horizontal Tabulation Set)
            TabStops_InsertStop(&self->hTabStops, self->x);
            break;

        case 0x8d:  // RI (Reverse Line Feed)
            Console_MoveCursor_Locked(self, kCursorMovement_AutoScroll, 0, -1);
            break;

        case 0x94:  // CCH (Cancel char)
            Console_Execute_BS_Locked(self);
#endif            
        default:
            // Ignore it
            break;
    }
}

static void Console_VT100_CSI_TBC_Locked(ConsoleRef _Nonnull self, int op)
{
    switch (op) {
        case 0:
            TabStops_RemoveStop(&self->hTabStops, self->x);
            break;

        case 3:
            TabStops_RemoveAllStops(&self->hTabStops);
            break;

        default:
            // Ignore
            break;
    }
}

static void Console_VT100_CSI_c_Locked(ConsoleRef _Nonnull self)
{
    const int p = get_nth_csi_parameter(self, 0, 0);

    switch (p) {
        case 0:
            Console_PostReport_Locked(self, "\033[?6c");   // VT100
            break;

        default:
            break;
    }
}

static void Console_VT100_CSI_h_Locked(ConsoleRef _Nonnull self)
{
    const bool isPrivateMode = has_private_use_char(self, '?');

    for (int i = 0; i < self->vtparser.vt500.num_params; i++) {
        const int p = get_nth_csi_parameter(self, i, 0);

        if (!isPrivateMode) {
            switch (p) {
                case 4: // IRM
                    self->flags.isInsertionMode = true;
                    break;

                default:
                    break;
            }
        }
        else {
            switch (p) {
                case 7: // DECAWM
                    self->flags.isAutoWrapEnabled = true;
                    break;

                case 25:
                    Console_SetCursorVisible_Locked(self, true);
                    break;

                default:
                    break;
            }
        }
    }
}

static void Console_VT100_CSI_l_Locked(ConsoleRef _Nonnull self)
{
    const bool isPrivateMode = has_private_use_char(self, '?');

    for (int i = 0; i < self->vtparser.vt500.num_params; i++) {
        const int p = get_nth_csi_parameter(self, i, 0);

        if (!isPrivateMode) {
            switch (p) {
                case 4: // IRM
                    self->flags.isInsertionMode = false;
                    break;

                default:
                    break;
            }
        }
        else {
            switch (p) {
                case 2: // VT52ANM
                    Console_SetCompatibilityMode_Locked(self, kCompatibilityMode_VT52);
                    break;

                case 7: // DECAWM
                    self->flags.isAutoWrapEnabled = false;
                    break;

                case 25:
                    Console_SetCursorVisible_Locked(self, false);
                    break;

                default:
                    break;
            }
        }
    }
}

static void Console_VT100_CSI_m_Locked(ConsoleRef _Nonnull self)
{
    for (int i = 0; i < self->vtparser.vt500.num_params; i++) {
        const int p = get_nth_csi_parameter(self, i, 0);

        switch (p) {
            case 0:     // Reset Character Attributes
                Console_ResetCharacterAttributes_Locked(self);
                break;

            case 1:     // Bold or increased intensity
                self->characterRendition.isBold = 1;
                break;

            case 2:     // Dimmed
                self->characterRendition.isDimmed = 1;
                break;

            case 3:     // Italic
                self->characterRendition.isItalic = 1;
                break;

            case 4:     // Underlined
                self->characterRendition.isUnderlined = 1;
                break;

            case 5:     // Blink
                self->characterRendition.isBlink = 1;
                break;

            case 7:     // Reverse
                self->characterRendition.isReverse = 1;
                break;

            case 8:     // Hidden
                self->characterRendition.isHidden = 1;
                break;

            case 9:     // Strikethrough
                self->characterRendition.isStrikethrough = 1;
                break;

            case 22:    // Reset Bold/Dimmed
                self->characterRendition.isBold = 0;
                break;

            case 23:    // Reset italic
                self->characterRendition.isItalic = 0;
                break;

            case 24:    // Reset Underlined
                self->characterRendition.isUnderlined = 0;
                break;

            case 25:    // Reset Blink
                self->characterRendition.isBlink = 0;
                break;

            case 27:    // Reset Reverse
                self->characterRendition.isReverse = 0;
                break;

            case 28:    // Reset Hidden
                self->characterRendition.isHidden = 0;
                break;

            case 29:    // Reset Strikethrough
                self->characterRendition.isStrikethrough = 0;
                break;

            case 30:    // Foreground color
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
                Console_SetForegroundColor_Locked(self, Color_MakeIndex(p - 30));
                break;

            case 39:    // Default Foreground color
                Console_SetDefaultForegroundColor_Locked(self);
                break;

            case 40:    // Background color
            case 41:
            case 42:
            case 43:
            case 44:
            case 45:
            case 46:
            case 47:
                Console_SetBackgroundColor_Locked(self, Color_MakeIndex(p - 40));
                break;

            case 49:    // Default Background color
                Console_SetDefaultBackgroundColor_Locked(self);
                break;

            default:
                break;
        }
    }
}

static void Console_VT100_CSI_n_Locked(ConsoleRef _Nonnull self)
{
    const bool isPrivateMode = has_private_use_char(self, '?');

    for (int i = 0; i < self->vtparser.vt500.num_params; i++) {
        const int p = get_nth_csi_parameter(self, i, 0);

        if (!isPrivateMode) {
            switch (p) {
                case 5: // Request status of terminal
                    Console_PostReport_Locked(self, "\033[0n");   // OK
                    break;

                case 6: { // Request cursor position
                    char numbuf[DIGIT_BUFFER_CAPACITY];
                    char buf[MAX_MESSAGE_LENGTH + 1];
                    char* ptr = &buf[0];
                    *ptr++ = '\033';
                    *ptr++ = '[';
                    if (self-> x > 0 || self->y > 0) {
                        const char* lb = Int32_ToString(self->y + 1, 10, false, numbuf);
                        while (*lb != '\0') {
                            *ptr++ = *lb++;
                        }
                        *ptr++ = ';';
                        const char* cb = Int32_ToString(self->x + 1, 10, false, numbuf);
                        while (*cb != '\0') {
                            *ptr++ = *cb++;
                        }
                    }
                    *ptr++ = 'R';
                    *ptr = '\0';
                    Console_PostReport_Locked(self, buf);
                    break;
                }

                default:
                    break;
            }
        }
        else {
            switch (p) {
                case 15: // Request status of printer
                    Console_PostReport_Locked(self, "\033[?13n");  // None
                    break;

                default:
                    break;
            }
        }
    }
}

static void Console_VT100_CSI_y_Locked(ConsoleRef _Nonnull self)
{
    if (get_nth_csi_parameter(self, 0, 0) != 2) {
        return;
    }

    switch (get_nth_csi_parameter(self, 1, 0)) {
        case 1:
        case 2:
        case 4:
        case 9:
        case 10:
        case 12:
        case 16:
        case 24:
            Console_PostReport_Locked(self, "\033[0n");   // OK
            break;

        default:
            break;
    }
}

static void Console_VT100_CSI_Locked(ConsoleRef _Nonnull self, unsigned char ch)
{
    switch (ch) {
        case 'c':   // DA
            Console_VT100_CSI_c_Locked(self);
            break;

        case 'f':   // HVP
            Console_MoveCursorTo_Locked(self, get_nth_csi_parameter(self, 1, 1) - 1, get_nth_csi_parameter(self, 0, 1) - 1);
            break;

        case 'g':   // TBC
            Console_VT100_CSI_TBC_Locked(self, get_csi_parameter(self, 0));
            break;

        case 'h':
            Console_VT100_CSI_h_Locked(self);
            break;

        case 'l':
            Console_VT100_CSI_l_Locked(self);
            break;

        case 'm':   // SGR
            Console_VT100_CSI_m_Locked(self);
            break;

        case 'n':   // DSR
            Console_VT100_CSI_n_Locked(self);
            break;

        case 'y':   // DECTST
            Console_VT100_CSI_y_Locked(self);
            break;

        case 'A':   // CUU
            Console_MoveCursor_Locked(self, kCursorMovement_Clamp, 0, -get_csi_parameter(self, 1));
            break;

        case 'B':   // CUD
            Console_MoveCursor_Locked(self, kCursorMovement_Clamp, 0, get_csi_parameter(self, 1));
            break;

        case 'C':   // CUF
            Console_MoveCursor_Locked(self, kCursorMovement_Clamp, get_csi_parameter(self, 1), 0);
            break;

        case 'D':   // CUB
            Console_MoveCursor_Locked(self, kCursorMovement_Clamp, -get_csi_parameter(self, 1), 0);
            break;

        case 'H':   // CUP
            Console_MoveCursorTo_Locked(self, 0, 0);
            break;

        case 'K':   // EL
            Console_ClearLine_Locked(self, self->y, (ClearLineMode) get_csi_parameter(self, 0));
            break;

        case 'J':   // ED
            Console_ClearScreen_Locked(self, (ClearScreenMode) get_csi_parameter(self, 0));
            break;

        case 'P':   // DCH
            Console_Execute_DCH_Locked(self, get_csi_parameter(self, 1));
            break;

        case 'L':   // IL
            Console_Execute_IL_Locked(self, get_csi_parameter(self, 1));
            break;

        case 'M':   // DL
            Console_Execute_DL_Locked(self, get_csi_parameter(self, 1));
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

static void Console_VT100_ESC_Locked(ConsoleRef _Nonnull self, unsigned char ch)
{
    switch (ch) {
        case 'D':   // ANSI: IND
            Console_MoveCursor_Locked(self, kCursorMovement_AutoScroll, 0, 1);
            break;

        case 'M':   // ANSI: RI
            Console_MoveCursor_Locked(self, kCursorMovement_AutoScroll, 0, -1);
            break;

        case 'E':   // ANSI: NEL
            Console_MoveCursor_Locked(self, kCursorMovement_AutoScroll, -self->x, 1);
            break;

        case '7':   // ANSI: DECSC
            Console_SaveCursorState_Locked(self);
            break;

        case '8':   // ANSI: DECRC
            Console_RestoreCursorState_Locked(self);
            break;

        case 'H':   // ANSI: HTS
            TabStops_InsertStop(&self->hTabStops, self->x);
            break;

        case 'Z':   // ANSI: DECID
            Console_PostReport_Locked(self, "\033[?6c");  // VT100
            break;

        case 'c':   // ANSI: RIS
            Console_ResetState_Locked(self);
            break;

        default:
            // Ignore
            break;
    }
}

void Console_VT100_ParseByte_Locked(ConsoleRef _Nonnull self, vt500parse_action_t action, unsigned char b)
{
    switch (action) {
        case VT500PARSE_ACTION_CSI_DISPATCH:
            Console_VT100_CSI_Locked(self, b);
            break;

        case VT500PARSE_ACTION_ESC_DISPATCH:
            Console_VT100_ESC_Locked(self, b);
            break;

        case VT500PARSE_ACTION_EXECUTE:
            Console_VT100_Execute_C0_Locked(self, b);
            break;

        case VT500PARSE_ACTION_PRINT:
            Console_PrintByte_Locked(self, b);
            break;

        default:
            // Ignore
            break;
    }
}
