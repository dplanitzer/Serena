//
//  mousecntl.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

static unsigned int g_mouse_cntl;


unsigned int ft_mousecntl(unsigned int mask)
{
    const unsigned int old_mask = g_mouse_cntl;
    const char* esc_seq;

    if ((mask & FT_ENABLED) == FT_ENABLED) {
        if ((mask & FT_MOTION) == FT_MOTION) {
            esc_seq = "\033[?1000h\033[?1003h\033[?1006h";
        }
        else {
            esc_seq = "\033[?1000h\033[?1006h";
        }
    }
    else {
        esc_seq = "\033[?1000l";
    }

    fputs(esc_seq, __ft_termout_fp);

    return old_mask;
}
