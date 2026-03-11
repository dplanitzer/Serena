//
//  isatty.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <fcntl.h>
#include <kpi/syscall.h>


int isatty(int fd)
{
    return (fcntl(fd, F_GETTYPE) == SEO_FT_TERMINAL) ? 1 : 0;
}
