//
//  _config_termin.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"
#include <serena/fd.h>

#define FLAGS_MASK  FT_NONBLOCKING

static int          g_termin_mode;
static unsigned int g_termin_flags;


void _ft_config_termin(unsigned int flags)
{    
    if (g_termin_flags != (flags & FLAGS_MASK)) {
        if ((flags & FT_NONBLOCKING) == FT_NONBLOCKING) {
            fd_setflags(__ft_termin_fd, FD_FOP_ADD, O_NONBLOCK);
        }
        else {
            fd_setflags(__ft_termin_fd, FD_FOP_REMOVE, O_NONBLOCK);
        }

        g_termin_flags = flags & FLAGS_MASK;
    }
}