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

#define NAME_ONLY 0
#define STEM_ONLY 1
#define EXT_ONLY  2

static ssize_t __basename_r(char* buf, size_t buf_size, const char* path, int mode)
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

    if (mode != NAME_ONLY && mode != STEM_ONLY && mode != EXT_ONLY) {
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

            // If the parsing option is other than full filename, scan for a valid extension dot
            if (!is_root && (mode == STEM_ONLY || mode == EXT_ONLY)) {
                ssize_t dot_idx = -1;
                
                // Scan backward from the end of the component to find the last '.'
                // It must have at least one char before it (i > start) and one after it (i < end)
                for (ssize_t i = end - 1; i > start; i--) {
                    if (path[i] == '.') {
                        dot_idx = i;
                        break;
                    }
                }

                if (dot_idx != -1) {
                    if (mode == STEM_ONLY) {
                        component_len = (size_t)(dot_idx - start);
                    } else if (mode == EXT_ONLY) {
                        start_idx = (size_t)(dot_idx + 1);
                        component_len = (size_t)(end - dot_idx);
                    }
                } else {
                    // No valid extension found
                    if (mode == EXT_ONLY) {
                        component_len = 0; // Extension is empty string
                    }
                    // For STEM_ONLY, if no extension exists, component_len remains full name len
                }
            }
            
            total_size += component_len;
        }
    }

    if (buf == NULL) {
        return (ssize_t)total_size;
    }

    if (total_size > buf_size) {
        errno = ENAMETOOLONG;
        if (buf_size > 0) {
            buf[0] = '\0';
        }
        return -1;
    }

    if (path_len == 0) {
        if (mode == EXT_ONLY) {
            buf[0] = '\0';
        } else {
            buf[0] = '.';
            buf[1] = '\0';
        }
    } else if (is_root) {
        if (mode == EXT_ONLY) {
            buf[0] = '\0';
        } else {
            buf[0] = '/';
            buf[1] = '\0';
        }
    } else {
        if (component_len > 0) {
            memcpy(buf, &path[start_idx], component_len);
        }
        buf[component_len] = '\0';
    }

    return 0;
}


ssize_t basename_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull path)
{
    return __basename_r(buf, buf_size, path, NAME_ONLY);
}

ssize_t stemname_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull path)
{
    return __basename_r(buf, buf_size, path, STEM_ONLY);
}

ssize_t extname_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull path)
{
    return __basename_r(buf, buf_size, path, EXT_ONLY);
}
