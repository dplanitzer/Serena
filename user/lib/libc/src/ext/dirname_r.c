//
//  dirname_r.c
//  libc
//
//  Created by Dietmar Planitzer on 10/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/path.h>
#include <string.h>
#include <errno.h>


ssize_t dirname_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull path)
{
    const char *start = path;
    size_t len = 0;

    if (*start == '\0') {
        start = ".";
        len = 1;
    } else {
        while (*start == '/') {
            start++;
        }

        // Back up by one so 'start' points to the slash we want to preserve
        if (start != path) {
            start--;
        }

        const char *end = start + strlen(start);

        // Strip trailing slashes: "a/b//" -> "a/b"
        while (end > start && *(end - 1) == '/') {
            end--;
        }

        // Skip over the last path component
        while (end > start && *(end - 1) != '/') {
            end--;
        }

        // Strip slashes before the trailing path component: "a//b" -> a
        while (end > start && *(end - 1) == '/') {
            end--;
        }

        // Calculate final length based on POSIX rules
        if (end == start) {
            // No slashes left or path was all slashes -> check original first char
            if (*start == '/') {
                len = 1; // It's the root directory "/"
            } else {
                start = "."; // It's a relative filename, directory is "."
                len = 1;
            }
        } else {
            len = end - start;
        }
    }

    const size_t required_size = len + 1; // Include space for '\0'

    if (buf == NULL) {
        return (ssize_t)required_size;
    }


    if (buf_size < required_size) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(buf, start, len);
    buf[len] = '\0';

    return 0;
}
