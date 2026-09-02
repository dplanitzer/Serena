//
//  color.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_fgcolor(int color)
{
    char* p = __ft_outbuf;

    if (color < FT_BLACK || color > FT_WHITE) {
        return;
    }

    *p++ = '\033';
    *p++ = '3';
    *p++ = color + '0';
    *p++ = 'm';
    *p = '\0';

    fputs(__ft_outbuf, __ft_termout_fp);
}

void ft_bgcolor(int color)
{
    char* p = __ft_outbuf;

    if (color < FT_BLACK || color > FT_WHITE) {
        return;
    }

    *p++ = '\033';
    *p++ = '4';
    *p++ = color + '0';
    *p++ = 'm';
    *p = '\0';

    fputs(__ft_outbuf, __ft_termout_fp);
}
