//
//  hline.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/3/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_hline(unsigned int ch, int count)
{
    if (ch == '\0' || count <= 0) {
        return;
    }

    int count8 = count >> 3;
    int count1 = count & 0x7;

    __ft_outbuf[0] = (char)ch;
    __ft_outbuf[1] = (char)ch;
    __ft_outbuf[2] = (char)ch;
    __ft_outbuf[3] = (char)ch;
    __ft_outbuf[4] = (char)ch;
    __ft_outbuf[5] = (char)ch;
    __ft_outbuf[6] = (char)ch;
    __ft_outbuf[7] = (char)ch;

    while (count8-- > 0) {
        fputs(__ft_outbuf, __ft_termout_fp);
    }
    if (count1 > 0) {
        __ft_outbuf[count1] = '\0';
        fputs(__ft_outbuf, __ft_termout_fp);
    }
}
