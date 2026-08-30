//
//  flowterm.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

static FILE* _Nonnull   __ft_termin_fp;
int                     __ft_termin_fd;
FILE* _Nonnull          __ft_termout_fp;


FILE* _Nonnull ft_termin(FILE* _Nonnull stream)
{
    FILE* old_fp = __ft_termin_fp;
    const int fd = fileno(stream);

    if (fd >= 0) {
        __ft_termin_fd = fd;
        __ft_termin_fp = stream;
        setvbuf(__ft_termin_fp, NULL, _IONBF, 0);
    }

    return old_fp;
}

FILE* _Nonnull ft_termout(FILE* _Nonnull stream)
{
    FILE* old_fp = __ft_termout_fp;

    __ft_termout_fp = stream;
    return old_fp;
}
