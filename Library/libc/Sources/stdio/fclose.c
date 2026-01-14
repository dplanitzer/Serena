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
        // First remove the stream from the global stream list
        __fdreg_file(s);

        // Now it's safe to destroy the stream
        __flock(s);
        r = __fclose(s);
        __setvbuf(s, NULL, _IONBF, 0);
        __funlock(s);
        mtx_deinit(&s->lock);

        if (s->flags.shouldFreeOnClose) {
            free(s);
        }
    }
    return r;
}
