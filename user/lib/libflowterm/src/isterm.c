//
//  isterm.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"
#include <serena/fd.h>

bool __ft_isterm(FILE* _Nullable s)
{
    const int fd = (s) ? fileno(s) : -1;

    return (fd >= 0 && fd_type(fd) == FD_TYPE_TERMINAL) ? true : false;
}

int ft_isterm(void)
{
    return __ft_isterm(termout);
}
