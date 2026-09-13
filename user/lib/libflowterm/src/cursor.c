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
    const char* csi;

    if (op == FT_ON) {
        csi = "\033[?25h";
    }
    else {
        csi = "\033[?25l";
    }

    fputs(csi, __ft_termout_fp);
}
