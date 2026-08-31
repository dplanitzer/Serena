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

    // Save cursor; move it to impossible location (will get clipped); query cursor position; restore cursor
    fputs("\0337\033[9999;9999f\033[6n\0338", __ft_termout_fp);
    fflush(__ft_termout_fp);
    
    if (ft_getevent(_FT_MSK_CURSOR_POSITION, 0, &evt) == 0) {
        *width = evt.data.cursor.x;
        *height = evt.data.cursor.y;
    }
    else {
        *width = 40;
        *height = 25;
    }
}
