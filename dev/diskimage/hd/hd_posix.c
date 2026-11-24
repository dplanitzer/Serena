//
//  hd_posix.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include "hd.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Returns what 'path' refers to (a link is followed), or KIND_ERROR with
 * errno set. The kernel applies a trailing slash itself: "file/" fails with
 * ENOTDIR, which is also what cp reports.
 */
int hd_path_kind(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0) {
        return (errno == ENOENT) ? HD_KIND_NONE : HD_KIND_ERROR;
    }
    return S_ISDIR(st.st_mode) ? HD_KIND_DIRECTORY : HD_KIND_OTHER;
}

int hd_create_dir(const char *path, int *created)
{
    if (mkdir(dst, 0755) == 0) {
        *created = 1;
        return 0;
    }
    if (errno != EEXIST) {
        return -1;
    }

    struct stat dst_st;
    if (stat(dst, &dst_st) != 0) {
        return -1;
    }
    if (!S_ISDIR(dst_st.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    *created = 0;
    return 0;
}

FILE *hd_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

#endif /* __POSIX__ */
