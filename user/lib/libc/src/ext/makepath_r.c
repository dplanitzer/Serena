//
//  makepath_r.c
//  libc
//
//  Created by Dietmar Planitzer on 10/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/path.h>
#include <ext/string.h>
#include <errno.h>


ssize_t makepath_r(char* _Nullable buf, size_t buf_size, const char* _Nullable dir, const char* _Nullable stem, const char* _Nullable ext)
{
    const size_t dir_len = dir ? strlen(dir) : 0;
    const size_t stem_len = stem ? strlen(stem) : 0;
    const size_t ext_len = ext ? strlen(ext) : 0;
    const size_t need_sep = (dir_len > 0 && dir[dir_len - 1] != '/') ? 1 : 0;
    const size_t need_dot = (stem_len > 0 && ext_len > 0) ? 1 : 0;

    const size_t total_needed = dir_len + need_sep + stem_len + need_dot + ext_len + 1;


    if (stem_len == 0 && ext_len > 0) {
        errno = EINVAL;
        if (buf_size > 0 && buf) {
            buf[0] = '\0';
        }
        return -1;
    }
    if (buf == NULL) {
        return total_needed;
    }
    if (buf_size < 1) {
        errno = EINVAL;
        return -1;
    }


    char *cur_pos = buf;
    size_t remaining_space = buf_size - 1; // reserve 1 byte for final '\0'

    *cur_pos = '\0';


    // Append directory component
    if (dir_len > 0) {
        const size_t limit = (dir_len > remaining_space) ? remaining_space : dir_len;
        
        cur_pos = strncat_x(cur_pos, dir, limit);
        remaining_space -= limit;

        // Append separator if needed and space allows
        if (need_sep && remaining_space > 0) {
            cur_pos = strncat_x(cur_pos, "/", 1);
            remaining_space -= 1;
        }
    }


    // Append stem component
    if (stem_len > 0 && remaining_space > 0) {
        const size_t limit = (stem_len > remaining_space) ? remaining_space : stem_len;
        
        cur_pos = strncat_x(cur_pos, stem, limit);
        remaining_space -= limit;
    }


    // Append extension separator, if needed
    if (ext_len > 0 && remaining_space > 0) {
        cur_pos = strcat_x(cur_pos, ".");
        remaining_space--;
    }


    // Append extension component
    if (ext_len > 0 && remaining_space > 0) {
        const size_t limit = (ext_len > remaining_space) ? remaining_space : ext_len;
        
        strncat_x(cur_pos, ext, limit);
    }


    return 0;
}
