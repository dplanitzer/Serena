//
//  strtobuf.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/28/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <kern/kernlib.h>
#include <string.h>

errno_t strtobuf(char* _Nonnull buf, size_t bufSize, const char* _Nonnull src)
{
    if (bufSize < 1) {
        return EINVAL;
    }

    const size_t len = strlen(src);
    if (bufSize < (len + 1)) {
        return ERANGE;
    }

    memcpy(buf, src, len);
    buf[len] = '\0';
    return EOK;
}
