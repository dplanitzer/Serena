//
//  style.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_style(unsigned int flags)
{
    char* p = __ft_outbuf;

    *p++ = '\033';
    *p++ = '0';

    if (flags != 0) {
        if ((flags & FT_BOLD) == FT_BOLD) {
            *p++ = ';';
            *p++ = '1';
        }
        if ((flags & FT_DIM) == FT_DIM) {
            *p++ = ';';
            *p++ = '2';
        }
        if ((flags & FT_ITALIC) == FT_ITALIC) {
            *p++ = ';';
            *p++ = '3';
        }
        if ((flags & FT_UNDERLINE) == FT_UNDERLINE) {
            *p++ = ';';
            *p++ = '4';
        }
        if ((flags & FT_BLINK) == FT_BLINK) {
            *p++ = ';';
            *p++ = '5';
        }
        if ((flags & FT_INVERSE) == FT_INVERSE) {
            *p++ = ';';
            *p++ = '7';
        }
        if ((flags & FT_HIDDEN) == FT_HIDDEN) {
            *p++ = ';';
            *p++ = '8';
        }
        if ((flags & FT_STRIKETHROUGH) == FT_STRIKETHROUGH) {
            *p++ = ';';
            *p++ = '9';
        }
    }

    *p++ = 'm';
    *p = '\0';

    fputs(__ft_outbuf, __ft_termout_fp);
}
