//
//  move.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"
#include <ext/math.h>

void ft_move(int dx, int dy)
{
    char* p = __ft_outbuf;

    if (dx != 0) {
        *p++ = '\033';
        *p++ = '[';
        p = __ft_itoa(__abs(dx), p);
        *p++ = (dx < 0) ? 'D' : 'C';
    }

    if (dy != 0) {
        *p++ = '\033';
        *p++ = '[';
        p = __ft_itoa(__abs(dy), p);
        *p++ = (dy < 0) ? 'A' : 'B';
    }
    
    *p = '\0';

    fputs(__ft_outbuf, __ft_termout_fp);
}
