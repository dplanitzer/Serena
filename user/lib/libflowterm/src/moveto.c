//
//  moveto.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_moveto(int x, int y)
{
    char* p = __ft_outbuf;

    *p++ = '\033';
    *p++ = '[';
    p = __ft_itoa(y, p);
    *p++ = ';';
    p = __ft_itoa(x, p);
    *p++ = 'f';
    *p = '\0';

    fputs(__ft_outbuf, __ft_termout_fp);
}
