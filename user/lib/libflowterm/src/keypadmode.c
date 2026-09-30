//
//  keypadmode.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/29/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"


void ft_keypadmode(int mode)
{
    const char* esc_seq;

    if (!__ft_termout_do_esc) {
        return;
    }


    switch (mode) {
        case FT_KEYPAD_NUMERIC:
            esc_seq = "\033>";
            break;

        case FT_KEYPAD_APP_MODE:
            esc_seq = "\033=";
            break;

        default:
            errno = EINVAL;
            return;
    }

    fputs(esc_seq, termout);
    fflush(termout);
}
