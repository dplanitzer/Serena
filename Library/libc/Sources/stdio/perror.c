//
//  perror.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdio.h>
#include <string.h>


void perror(const char *str)
{
    if (str && *str != '\0') {
        fputs(str, stdout);
        fputs(": ", stdout);
    }

    puts(strerror(errno));
}
