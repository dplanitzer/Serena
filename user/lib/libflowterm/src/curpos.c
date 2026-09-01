//
//  curpos.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"


void ft_curpos(int* _Nonnull x, int* _Nonnull y)
{
    ft_event_t evt;

    fputs("\033[6n", __ft_termout_fp);
    fflush(__ft_termout_fp);
    
    if (ft_getevent(_FT_MSK_CURSOR_POSITION, 0, &evt) == 0) {
        *x = evt.data.report.param[1];
        *y = evt.data.report.param[0];
    }
    else {
        *x = 1;
        *y = 1;
    }
}
