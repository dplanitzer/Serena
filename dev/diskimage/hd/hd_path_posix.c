//
//  hd_path_posix.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include "hd_path.h"
#include <string.h>

bool hd_ends_with_separator(const char *path)
{
    const size_t len = strlen(path);

    return len > 0 && path[len - 1] == '/';
}

int hd_path_append(char* buf, size_t buf_size, const char* component)
{
    size_t base_len = 0;
    size_t needs_sep = 0;

    if (buf) {
        base_len = strlen(buf);
        if (base_len > 0 && buf[base_len - 1] != '/') {
            needs_sep = 1;
        }
    }

    // Leading separators are only dropped when appending to a path. Appended
    // to an empty path, a component stays as it is, so "/usr" stays absolute.
    if (base_len > 0) {
        while (*component == '/') {
            component++;
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

int hd_last_path_component(char* buf, size_t buf_size, const char* path)
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
        int end = (int)path_len - 1;
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
            int start = end;

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
        return (int)total_size;
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

int hd_path_next(hd_path_iter_t* it, const char* path)
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

#endif /* __POSIX__ */
