//
//  cmdlib.c
//  sh
//
//  Created by Dietmar Planitzer on 5/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "cmdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void print_error(const char* _Nonnull cmd, const char* _Nullable path, errno_t err)
{
    const char* errstr = strerror(err);

    if (path && path[0] != '\0') {
        printf( "%s: %s: %s\n", cmd, path, errstr);
    }
    else {
        printf("%s: %s\n", cmd, errstr);
    }
}
