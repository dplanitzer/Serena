//
//  relpath_r.c
//  libc
//
//  Created by Dietmar Planitzer on 11/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/path.h>
#include <string.h>
#include <errno.h>

ssize_t relpath_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull path_prefix, const char* _Nonnull path)
{
    const char* rel_suffix = path;

    if (buf != NULL && buf_size < 1) {
        errno = EINVAL;
        return -1;
    }


    // Return the original path if 'path_prefix' is the empty string
    if (*path_prefix != '\0') {
        const char* p = path_prefix;
        const char* s = path;

        while (*p != '\0' && *s != '\0') {
            if (*p == '/') {
                // If prefix hits a slash sequence, path must also hit a slash sequence
                if (*s != '/') {
                    break;
                }

                // Collapse consecutive slashes in 'path_prefix' and 'path'
                while (*p == '/') p++;
                while (*s == '/') s++;
            }
            else {
                if (*p != *s) {
                    break;
                }

                p++;
                s++;
            }
        }

        // Collapse any trailing slashes remaining in 'path_prefix'
        while (*p == '/') p++;

        if (*p == '\0') {
            // A structural match requires that 's' is either:
            // 1. At the end of the string '\0'
            // 2. Pointing to a new directory component (meaning the character 
            //    just before it was a '/', or 's' itself points to a '/')
            if (*s == '\0') {
                rel_suffix = s;
            }
            else if (*s == '/' || (s > path && *(s - 1) == '/')) {
                // Advance past any remaining slashes to find the pure relative suffix
                while (*s == '/') {
                    s++;
                }

                rel_suffix = s;
            }
        }
    }

    // Calculate required space (including null terminator)
    const size_t suffix_len = strlen(rel_suffix);
    const size_t required_len = suffix_len + 1;

    if (buf == NULL) {
        return required_len;
    }


    if (required_len > buf_size) {
        errno = ENAMETOOLONG;
        return -1;
    }


    if (suffix_len > 0) {
        memcpy(buf, rel_suffix, suffix_len);
    }
    buf[suffix_len] = '\0';

    return 0;
}