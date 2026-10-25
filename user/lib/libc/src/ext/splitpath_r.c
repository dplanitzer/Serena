//
//  splitpath_r.c
//  libc
//
//  Created by Dietmar Planitzer on 10/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <string.h>


void splitpath_r(const char* _Nonnull path, char* _Nullable dir, size_t dir_len, char* _Nullable stem, size_t stem_len, char* _Nullable ext, size_t ext_len)
{
    const char *last_slash = strrchr(path, '/');
    const char *filename_start = last_slash ? last_slash + 1 : path;

    // Extract the directory component
    if (dir && dir_len > 0) {
        if (last_slash) {
            size_t len = last_slash - path + 1; // include trailing slash
            
            if (len >= dir_len) {
                len = dir_len - 1;
            }

            strncpy(dir, path, len);
            dir[len] = '\0';
        } else {
            // no directory component
            dir[0] = '\0';
        }
    }


    // Find the last extension dot in the filename component.
    // Note that its only an extension if there is a character in front of it that isn't a slash
    const char *last_dot = strrchr(filename_start, '.');
    if (last_dot == path || *(last_dot - 1) == '/') {
        last_dot = NULL;
    }


    // Extract the stem component
    if (stem && stem_len > 0) {
        size_t len = last_dot ? (size_t)(last_dot - filename_start) : strlen(filename_start);
        
        if (len >= stem_len) {
            len = stem_len - 1;
        }

        strncpy(stem, filename_start, len);
        stem[len] = '\0';
    }


    // Extract the extension component
    if (ext && ext_len > 0) {
        if (last_dot) {
            strncpy(ext, last_dot + 1, ext_len - 1);
            ext[ext_len - 1] = '\0';
        } else {
            // No extension component
            ext[0] = '\0';
        }
    }
}
