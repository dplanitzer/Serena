//
//  utils.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <stdlib.h>
#include <string.h>


// Creates a destination path like this:
// - 'path' does not end in a '/': path
// - 'path' does end in a '/': path/<filename_component_of_srcPath>
// The returned path must be freed with free().
char* _Nullable create_dst_path(const char* _Nonnull srcPath, const char* _Nonnull path)
{
    const size_t pathLen = strlen(path);
    const char lastPathChar = path[pathLen - 1];

    if (lastPathChar == '/' || lastPathChar == '\\') {
        char* filenameStart = strrchr(srcPath, '/');

        if (filenameStart == NULL) {
            filenameStart = strrchr(srcPath, '\\');
        }

        if (filenameStart) {
            const size_t filenameLen = strlen(filenameStart + 1);
            char* dstPath = malloc(pathLen + filenameLen + 1);

            memcpy(dstPath, path, pathLen);
            memcpy(&dstPath[pathLen], filenameStart + 1, filenameLen);
            dstPath[pathLen + filenameLen] = '\0';

            return dstPath;
        }
    }

    return strdup(path);
}

#if defined(__APPLE__)
char * _Nullable strdup(const char * _Nonnull src)
{
    const size_t lenWithNul = strlen(src) + 1;
    char* dst = malloc(lenWithNul);

    if (dst) {
        memcpy(dst, src, lenWithNul);
    }
    return dst;
}
#endif
