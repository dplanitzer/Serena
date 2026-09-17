//
//  fillrect.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/16/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_fillrect(unsigned int ch, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }


    // escape sequence to move the cursor back 'width' characters and down one line
    char* p = __ft_outbuf;
    *p++ = '\033';
    *p++ = '[';
    p = __ft_itoa(width, p);
    *p++ = 'D';
    *p++ = '\033';
    *p++ = '[';
    *p++ = 'B';
    *p++ = '\0';


    for (int y = 0; y < height - 1; y++) {
        ft_hline(ch, width);
        fputs(__ft_outbuf, __ft_termout_fp);
    }

    ft_hline(ch, width);
}

void ft_clearrect(int width, int height)
{
    ft_fillrect(' ', width, height);
}
