//
//  screensize.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"


void ft_screensize(int* _Nonnull width, int* _Nonnull height)
{
    ft_event_t evt;

    if (!__ft_termout_do_esc) {
        *width = 40;
        *height = 25;
        return;
    }

    
    // Save cursor; move it to impossible location (will get clipped); query cursor position; restore cursor
    fputs("\0337\033[9999;9999f\033[6n\0338", termout);
    fflush(termout);
    
    if (ft_getevent(_FT_MSK_CURSOR_POSITION, 0, &evt) == 0) {
        *width = evt.data.report.param[1];
        *height = evt.data.report.param[0];
    }
    else {
        *width = 40;
        *height = 25;
    }
}
