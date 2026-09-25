//
//  vline.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/4/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_vline(unsigned int ch, int count)
{
    if (!__ft_termout_do_esc || ch == '\0' || count <= 0) {
        return;
    }

    __ft_outbuf[0] = (char)ch;
    __ft_outbuf[1] = '\033';
    __ft_outbuf[2] = '[';
    __ft_outbuf[3] = 'B';
    __ft_outbuf[4] = '\033';
    __ft_outbuf[5] = '[';
    __ft_outbuf[6] = 'D';

    // avoid triggering a v-scroll by not doing any v-cursor movement after the
    // last character has been printed
    while (count-- > 1) {
        fputs(__ft_outbuf, termout);
    }
    putc(ch, termout);
}
