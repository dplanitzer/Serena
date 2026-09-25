//
//  save_restore.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_save(void)
{
    if (__ft_termout_do_esc) {
        fputs("\0337", termout);
    }
}

void ft_restore(void)
{
    if (__ft_termout_do_esc) {
        fputs("\0338", termout);
    }
}
