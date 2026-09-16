//
//  cleanup.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"
#include <serena/fd.h>


void ft_cleanup(void)
{
    if (__ft_termin_fd >= 0) {
        fd_setflags(__ft_termin_fd, FD_FOP_REMOVE, O_NONBLOCK);
    }

    // Turn cursor back on and change the colors back to black bg and green fg
    fputs("\033[?25h", __ft_termout_fp);
    fputs("\033[32m\033[40m", __ft_termout_fp);
    fflush(__ft_termout_fp);
}
