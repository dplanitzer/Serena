//
//  Stream.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


int fflush(FILE *s)
{
    if (s) {
        return __fflush(s);
    }
    else {
        return __iterate_open_files(__fflush);
    }
}
