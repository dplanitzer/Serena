//
//  insertmode.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/19/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_insertmode(int op)
{
    if (__ft_termout_do_esc) {
        fputs((op == FT_ON) ? "\033[4h" : "\033[4l", __ft_termout_fp);
    }
}
