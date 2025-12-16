//
//  strrchr.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stddef.h>


char * _Nullable strrchr(const char * _Nonnull str, int ch)
{
    char *p = strchr(str, ch);

    if (ch == '\0' || p == NULL) {
        return p;
    }

    for (;;) {
        char *q = strchr(p + 1, ch);

        if (q == NULL) {
            break;
        }
        p = q;
    }

    return p;
}
