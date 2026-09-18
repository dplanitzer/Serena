//
//  cursor.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_cursor(int op)
{
    if (__ft_termout_do_esc) {
        const char* csi = (op == FT_ON) ? "\033[?25h" : "\033[?25l";

        fputs(csi, __ft_termout_fp);
    }
}
