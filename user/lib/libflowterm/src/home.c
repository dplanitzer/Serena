//
//  home.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/3/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_home(void)
{
    if (__ft_termout_do_esc) {
        fputs("\033[H", __ft_termout_fp);
    }
}
