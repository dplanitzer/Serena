//
//  perror.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <string.h>


void perror(const char *str)
{
    if (str && *str != '\0') {
        puts(str);
        puts(": ");
    }

    puts(strerror(errno));
}
