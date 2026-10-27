//
//  pathjoin_r.c
//  libc
//
//  Created by Dietmar Planitzer on 10/26/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/path.h>
#include <string.h>
#include <errno.h>


ssize_t pathjoin_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull component)
{
    while (*component == '/') {
        component++;
    }

    size_t base_len = 0;
    size_t needs_sep = 0;

    if (buf) {
        base_len = strlen(buf);
        if (base_len > 0 && buf[base_len - 1] != '/') {
            needs_sep = 1;
        }
    }


    const size_t comp_len = strlen(component);

    // If the component was just slashes, no new path characters are added, 
    // but a separator might still need to be accounted for if it was deemed necessary.
    if (comp_len == 0) {
        needs_sep = 0;
    }

    const size_t total_size = base_len + needs_sep + comp_len + 1;

    if (buf == NULL) {
        return total_size;
    }

    if (total_size > buf_size) {
        errno = ENAMETOOLONG;
        return -1;
    }


    if (needs_sep) {
        buf[base_len] = '/';
        base_len++;
    }

    if (comp_len > 0) {
        memcpy(&buf[base_len], component, comp_len);
    }
    buf[base_len + comp_len] = '\0';

    return 0;
}
