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
    ft_event_t evt;

    fputs("\033[5n", __ft_termout_fp);
    fflush(__ft_termout_fp);
    
    if (ft_getevent(_FT_MSK_STATUS, 0, &evt) == 0) {
        return evt.data.report.param[0];
    }
    else {
        return -1;
    }
}
