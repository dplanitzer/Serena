//
//  cls.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/3/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_cls(void)
{
    const char* str;

    if (__ft_termout_do_esc) {
        str = "\033[2J\033[H";
    }
    else {
        str = "\n\n";
    }

    fputs(str, __ft_termout_fp);
}
