//
//  sizepath_r.c
//  libc
//
//  Created by Dietmar Planitzer on 10/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/path.h>
#include <string.h>
#include <stddef.h>


void sizepath_r(const char* _Nonnull path, size_t* _Nullable dir_len, size_t* _Nullable stem_len, size_t* _Nullable ext_len)
{
    const char *last_slash = strrchr(path, '/');
    const char *filename_start = last_slash ? last_slash + 1 : path;

    // Find the last extension dot in the filename component.
    const char *last_dot = strrchr(filename_start, '.');
    if (last_dot && (last_dot == path || *(last_dot - 1) == '/')) {
        last_dot = NULL;
    }


    // Calculate the directory buffer size
    if (dir_len) {
        if (last_slash) {
            // Number of chars up to and including the slash + 1 for the trailing '\0'
            *dir_len = (size_t)(last_slash - path) + 1 + 1;
        } else {
            *dir_len = 1;
        }
    }


    // Calculate the stem buffer size
    if (stem_len) {
        const size_t len = last_dot ? (size_t)(last_dot - filename_start) : strlen(filename_start);
        
        *stem_len = len + 1;
    }


    // Calculate the extension buffer size
    if (ext_len) {
        if (last_dot) {
            // Characters after the dot + 1 for teh trailing '\0'
            *ext_len = strlen(last_dot + 1) + 1;
        } else {
            *ext_len = 1;
        }
    }
}
