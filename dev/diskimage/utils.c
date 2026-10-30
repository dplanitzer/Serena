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

char g_copy_buf[COPY_BUF_SIZE];
char g_host_path[HOST_PATH_MAX];
char g_dmg_path[DMG_PATH_MAX];
char g_last_path_comp[LPC_SIZE];


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

#ifdef _WIN32
#include <Windows.h>

bool is_host_directory(const char* _Nonnull path)
{
    // Try to convert assuming UTF-8 encoding
    int wide_chars_needed = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    UINT code_page = CP_UTF8;

    // If UTF-8 parsing failed, fall back to the system ANSI code page
    if (wide_chars_needed <= 0) {
        wide_chars_needed = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
        code_page = CP_ACP;
    }

    if (wide_chars_needed <= 0) {
        return false;
    }

    wchar_t* wide_path = malloc(wide_chars_needed * sizeof(wchar_t));
    if (wide_path == NULL) {
        return false;
    }

    // Perform the actual conversion to Wide Characters
    if (MultiByteToWideChar(code_page, 0, path, -1, wide_path, wide_chars_needed) == 0) {
        free(wide_path);
        return false;
    }

    DWORD attributes = GetFileAttributesW(wide_path);
    
    free(wide_path);

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false; 
    }
    
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
#else
#include <sys/types.h>
#include <sys/stat.h>

bool is_host_directory(const char* _Nonnull path)
{
    struct stat path_stat;
    
    if (stat(path, &path_stat) != 0) {
        return false; 
    }
    
    return S_ISDIR(path_stat.st_mode);
}
#endif

errno_t set_host_path(const char* _Nonnull path)
{
    const size_t len = strlen(path);

    if (len > HOST_PATH_MAX-1) {
        return ENAMETOOLONG;
    }

    memcpy(g_host_path, path, len);
    g_host_path[len] = '\0';

    return EOK;
}

errno_t set_dmg_path(const char* _Nonnull path)
{
    const size_t len = strlen(path);

    if (len > DMG_PATH_MAX-1) {
        return ENAMETOOLONG;
    }

    memcpy(g_dmg_path, path, len);
    g_dmg_path[len] = '\0';

    return EOK;
}

static di_path_type concrete_path_type(di_path_type path_type)
{
    switch (path_type) {
        case HOST_PATH:
#ifdef _WIN32
            return WIN32_PATH;
#else
            return POSIX_PATH;
#endif

        case DMG_PATH:
            return POSIX_PATH;

        default:
            return path_type;
    }
}

static int is_separator(di_path_type path_type, char ch)
{
    if (path_type == WIN32_PATH) {
        return ch == '/' || ch == '\\';
    }
    else {
        return ch == '/';
    }
}

ssize_t path_append(di_path_type path_type, char* _Nullable buf, size_t buf_size, const char* _Nonnull component)
{
    const di_path_type cpath_type = concrete_path_type(path_type);

    while (is_separator(cpath_type, *component)) {
        component++;
    }

    size_t base_len = 0;
    size_t needs_sep = 0;

    if (buf) {
        base_len = strlen(buf);
        if (base_len > 0 && !is_separator(cpath_type, buf[base_len - 1])) {
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
        buf[base_len] = (cpath_type == WIN32_PATH) ? '\\' : '/';
        base_len++;
    }

    if (comp_len > 0) {
        memcpy(&buf[base_len], component, comp_len);
    }
    buf[base_len + comp_len] = '\0';

    return 0;
}

ssize_t last_path_component(di_path_type path_type, char* _Nullable buf, size_t buf_size, const char* _Nonnull path)
{
    const di_path_type cpath_type = concrete_path_type(path_type);
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
        while (end > 0 && is_separator(cpath_type, path[end])) {
            end--;
        }

        if (end == 0 && is_separator(cpath_type, path[0])) {
            // Root path case -> "/" requires 2 bytes ('/' and '\0')
            total_size++;
            is_root = true;
        }
        else {
            // Find the start of the last component by moving backward to the next slash
            ssize_t start = end;

            while (start >= 0 && !is_separator(cpath_type, path[start])) {
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
        buf[0] = (cpath_type == WIN32_PATH) ? '\\' : '/';
        buf[1] = '\0';
    } else {
        memcpy(buf, &path[start_idx], component_len);
        buf[component_len] = '\0';
    }

    return 0;
}
