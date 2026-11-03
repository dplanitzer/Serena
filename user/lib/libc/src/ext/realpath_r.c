//
//  realpath_r.c
//  libc
//
//  Created by Dietmar Planitzer on 11/2/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/path.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <serena/file.h>
#include <serena/process.h>
#include <kpi/syslimits.h>

int realpath_r(char* _Nonnull buf, size_t buf_size, const char* _Nonnull path)
{
    fs_attr_t attr;
    path_iter_t it = PATH_ITER_INITIALIZER;
    const size_t path_len = strlen(path);
    char* p = buf;
    char* ep = &p[buf_size];

    if (buf_size < 1) {
        errno = EINVAL;
        return -1; 
    }

    *p = '\0';

    if (path_len >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (path_len == 0) {
        errno = ENOENT;
        return -1;
    }
    
    
    // Convert the path to an absolute path and make sure that we always start
    // out with a '/' in the result buffer
    if (*path != '/') {
        if (proc_cwd(p, buf_size) < 0) {
            errno = ENAMETOOLONG;
            return -1;
        }

        p += strlen(p);
    }

    if (*p != '/') {
        if ((ep - p) < 2) {
            errno = ENAMETOOLONG;
            return -1;
        }

        *p++ = '/';
        *p   = '\0';
    }


    // Iterate the path components and build the resolved path component by
    // component
    while (!pathnext(&it, path)) {
        if (it.length == 1 && it.start[0] == '.') {
            continue;
        }
        if (it.length == 2 && it.start[0] == '.' && it.start[1] == '.') {
            // Erase the most recently written path component
            while (p > buf && *p != '/') {
                p--;
            }
            *p = '\0';
            
            continue;
        }


        // Append the current path component to the result buffer
        const size_t needs_sep = (it.index > 0) ? 1 : 0;
        const size_t needed_bytes = needs_sep + it.length + 1;

        if ((ep - p) < needed_bytes) {
            errno = ENAMETOOLONG;
            return -1;
        }

        if (needs_sep) {
            // Note that the '/' for component #0 was written at the start of
            // this function (see above)
            *p++ = '/';
        }
        memcpy(p, it.start, it.length);
        
        p += it.length;
        *p = '\0';


        // Verify that the path component exists and check whether it's a
        // symlink
        if (fs_attr(NULL, buf, &attr) < 0) {
            return -1;
        }
    //    if (attr.file_type == FS_FTYPE_LNK) {
            //XXX replace the current path component with the resolved symlink path
    //    }
    }
    

    // Validate that the last path component is actually a directory if 'path'
    // ends in a '/' character
    if (it.index >= 0 && path[path_len - 1] == '/' && attr.file_type != FS_FTYPE_DIR) {
        errno = ENOTDIR;
        return -1;
    }
    
    return 0;
}
