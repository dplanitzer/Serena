//
//  movetox.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/28/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"
#include <ext/math.h>

void ft_movetox(int x)
{
    if (!__ft_termout_do_esc) {
        return;
    }
    if (x <= 0) {
        errno = EINVAL;
        return;
    }

    
    char* p = __ft_outbuf;

    *p++ = '\r';
    if (x > 1) {
        *p++ = '\033';
        *p++ = '[';
        if (x > 2) {
            p = __ft_itoa(x - 1, p);
        }
        *p++ = 'C';
    }
    *p = '\0';

    fputs(__ft_outbuf, termout);
}
