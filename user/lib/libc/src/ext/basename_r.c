//
//  basename_r.c
//  libc
//
//  Created by Dietmar Planitzer on 10/26/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/path.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>


ssize_t basename_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull path)
{
    const size_t path_len = strlen(path);
    size_t total_size = 1;
    size_t component_len = 0;
    size_t start_idx = 0;
    bool is_root = false;

    if (buf && buf_size < 1) {
        errno = EINVAL;
        return -1;
    }

    
    // Calculate result path size
    if (path_len == 0) {
        // Empty path case -> requires 2 bytes ('.' and '\0')
        total_size++;
    }
    else {
        // Strip trailing slashes to find the true end of the last component
        ssize_t end = (ssize_t)path_len - 1;
        while (end > 0 && path[end] == '/') {
            end--;
        }

        if (end == 0 && path[0] == '/') {
            // Root path case -> "/" requires 2 bytes ('/' and '\0')
            total_size++;
            is_root = true;
        }
        else {
            // Find the start of the last component by moving backward to the next slash
            ssize_t start = end;

            while (start >= 0 && path[start] != '/') {
                start--;
            }
            start++; // Now points to the first character of the path component
            
            start_idx = (size_t)start;
            component_len = (size_t)(end - start + 1);
            total_size += component_len;
        }
    }

    if (buf == NULL) {
        return (ssize_t)total_size;
    }


    if (total_size > buf_size) {
        errno = ENAMETOOLONG;
        buf[0] = '\0';
        return -1;
    }


    if (path_len == 0) {
        buf[0] = '.';
        buf[1] = '\0';
    } else if (is_root) {
        buf[0] = '/';
        buf[1] = '\0';
    } else {
        memcpy(buf, &path[start_idx], component_len);
        buf[component_len] = '\0';
    }

    return 0;
}
