//
//  utils.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "utils.h"
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
char * _Nullable strdup(const char * _Nonnull src)
{
    const size_t lenWithNul = strlen(src) + 1;
    char* dst = malloc(lenWithNul);

    if (dst) {
        memcpy(dst, src, lenWithNul);
    }
    return dst;
}
#endif
