//
//  utils.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "path_utils.h"
#include <stdlib.h>
#include <string.h>

char g_copy_buf[COPY_BUF_SIZE];
char g_host_path[HOST_PATH_MAX];
char g_dmg_path[DMG_PATH_MAX];
char g_last_path_comp[LPC_SIZE];

errno_t host_set_path(const char* _Nonnull path)
{
    const size_t len = strlen(path);

    if (len > HOST_PATH_MAX-1) {
        return ENAMETOOLONG;
    }

    memcpy(g_host_path, path, len);
    g_host_path[len] = '\0';

    return EOK;
}

errno_t dmg_set_path(const char* _Nonnull path)
{
    const size_t len = strlen(path);

    if (len > DMG_PATH_MAX-1) {
        return ENAMETOOLONG;
    }

    memcpy(g_dmg_path, path, len);
    g_dmg_path[len] = '\0';

    return EOK;
}

#if defined(__APPLE__)

#include <sys/types.h>
#include <sys/stat.h>

bool posix_is_directory(const char* _Nonnull path)
{
    struct stat path_stat;
    
    if (stat(path, &path_stat) != 0) {
        return false; 
    }
    
    return S_ISDIR(path_stat.st_mode);
}

#endif /* __APPLE__ */

ssize_t posix_path_append(char* _Nullable buf, size_t buf_size, const char* _Nonnull component)
{
    while (*component == '/') {
        component++;
    }

    size_t base_len = 0;
    size_t needs_sep = 0;

    if (buf) {
        base_len = strlen(buf);
        if (base_len > 0 && buf[base_len - 1] != '/') {
            needs_sep = 1;
        }
    }


    const size_t comp_len = strlen(component);

    // If the component was just slashes, no new path characters are added, 
    // but a separator might still need to be accounted for if it was deemed necessary.
    if (comp_len == 0) {
        needs_sep = 0;
    }

    const size_t total_size = base_len + needs_sep + comp_len + 1;

    if (buf == NULL) {
        return total_size;
    }

    if (total_size > buf_size) {
        errno = ENAMETOOLONG;
        return -1;
    }


    if (needs_sep) {
        buf[base_len] = '/';
        base_len++;
    }

    if (comp_len > 0) {
        memcpy(&buf[base_len], component, comp_len);
    }
    buf[base_len + comp_len] = '\0';

    return 0;
}

ssize_t posix_last_path_component(char* _Nullable buf, size_t buf_size, const char* _Nonnull path)
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

int posix_path_next(di_path_iter_t* _Nonnull it, const char* _Nonnull path)
{
    // First-time initialization
    if (it->_next_ptr == NULL) {
        it->_next_ptr = path;
        it->index = -1;
    }

    // Find the start of the next path component
    while (*(it->_next_ptr) == '/') {
        it->_next_ptr++;
    }

    if (*(it->_next_ptr) == '\0') {
        it->start = NULL;
        it->end = NULL;
        it->length = 0;
        return 1;
    }

    it->start = it->_next_ptr;
    it->index++;

    // Find the end of the next path component
    while (*(it->_next_ptr) != '\0' && *(it->_next_ptr) != '/') {
        it->_next_ptr++;
    }
    it->end = it->_next_ptr;
    it->length = it->end - it->start;

    return 0;
}
