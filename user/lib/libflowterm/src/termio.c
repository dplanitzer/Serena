//
//  termio.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"
#include <serena/fd.h>

FILE* _Nonnull  termin;
FILE* _Nonnull  termout;

int     __ft_termin_fd = -1;    // >= 0 if valid; -1 if not valid
bool    __ft_termout_do_esc;    // true if escape sequence capable; false otherwise


int ft_settermin(FILE* _Nonnull stream)
{
    const int fd = fileno(stream);

    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }


    __ft_termin_fd = fd;
    termin = stream;
    setvbuf(termin, NULL, _IONBF, 0);
    
    return 0;
}

int ft_settermout(FILE* _Nonnull stream)
{
    termout = stream;
    __ft_termout_do_esc = __ft_isterm(stream);

    return 0;
}
