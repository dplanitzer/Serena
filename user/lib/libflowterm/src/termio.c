//
//  termio.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"
#include <serena/fd.h>

static FILE* _Nullable  __ft_termin_fp;
int                     __ft_termin_fd = -1;    // >= 0 if valid; -1 if not valid
FILE* _Nonnull          __ft_termout_fp;
bool                    __ft_termout_do_esc;    // true if escape sequence capable; false otherwise


static bool __ft_isterm(FILE* _Nullable s)
{
    const int fd = (s) ? fileno(s) : -1;

    return (fd >= 0 && fd_type(fd) == FD_TYPE_TERMINAL) ? true : false;
}

FILE* _Nullable ft_termin(FILE* _Nonnull stream)
{
    FILE* old_fp = __ft_termin_fp;
    const int fd = fileno(stream);

    if (fd >= 0) {
        __ft_termin_fd = fd;
        __ft_termin_fp = stream;
        setvbuf(__ft_termin_fp, NULL, _IONBF, 0);
    }
    else {
        __ft_termin_fd = -1;
        __ft_termin_fp = NULL;
    }

    return old_fp;
}

FILE* _Nonnull ft_termout(FILE* _Nonnull stream)
{
    FILE* old_fp = __ft_termout_fp;

    __ft_termout_fp = stream;
    __ft_termout_do_esc = __ft_isterm(stream);

    return old_fp;
}

int ft_isterm(void)
{
    return __ft_isterm(__ft_termout_fp);
}
