//
//  pathcopy_r.c
//  libc
//
//  Created by Dietmar Planitzer on 10/29/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/path.h>
#include <errno.h>


int pathcopy_r(char* _Nonnull buf, size_t buf_size, const char* _Nonnull path)
{
    if (buf_size == 0) {
        errno = EINVAL;
        return -1;
    }


    size_t i = 0;
    
    while (path[i] != '\0') {
        if (i >= buf_size - 1) {
            errno = ENAMETOOLONG;
            return -1;
        }

        buf[i] = path[i];
        i++;
    }
    
    buf[i] = '\0';

    return 0;
}
