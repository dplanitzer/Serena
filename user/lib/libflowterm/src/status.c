//
//  status.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/31/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"


int ft_status(void)
{
    if (__ft_termout_do_esc) {
        ft_event_t evt;

        fputs("\033[5n", termout);
        fflush(termout);
    
        if (ft_getevent(_FT_MSK_STATUS, 0, &evt) == 0) {
            return evt.data.report.param[0];
        }
    }

    return -1;
}
