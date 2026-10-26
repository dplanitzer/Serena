//
//  fullpath_r.c
//  libc
//
//  Created by Dietmar Planitzer on 10/25/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/path.h>
#include <ext/string.h>
#include <errno.h>
#include <stddef.h>
#include <serena/process.h>


char* _Nullable fullpath_r(char* _Nonnull buf, size_t buf_size, const char* _Nullable rel_path)
{
    if (buf_size < 1) {
        errno = EINVAL;
        return NULL; 
    }

    const size_t rel_len = (rel_path) ? strlen(rel_path) : 0;

    if (rel_len == 0) {
        rel_path = "."; 
    }


    // Case #1: relative path is actually absolute
    if (rel_path[0] == '/') {
        if (rel_len + 1 >= buf_size) {
            errno = ERANGE;
            return NULL;
        }

        strcpy_x(buf, rel_path);
        return buf;
    }


    // Case #2: 'rel_path' is actually relative. Read CWD directly into 'buf'.
    // and then append 'rel_path' to it.
    if (proc_cwd(buf, buf_size) < 0) {
        errno = ERANGE;
        return NULL;
    }

    const size_t cwd_len = strlen(buf);
    const int needs_sep = (buf[cwd_len - 1] == '/') ? 0 : 1;

    if (cwd_len + needs_sep + rel_len + 1 >= buf_size) {
        buf[0] = '\0';
        errno = ERANGE;
        return NULL; 
    }


    char* end_ptr = buf + cwd_len;
    if (needs_sep) {
        end_ptr = strcpy_x(end_ptr, "/");
    }
    strcpy_x(end_ptr, rel_path);

    return buf;
}
