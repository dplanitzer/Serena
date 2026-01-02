//
//  fclose.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdlib.h>


int fclose(FILE * _Nonnull s)
{
    int r = 0;

    if (s) {
        r = __fclose(s);

        if (s->flags.shouldFreeOnClose) {
            free(s);
        }
    }
    return r;
}
