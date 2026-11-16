//
//  path_utils.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef path_utils_h
#define path_utils_h

#include <stdbool.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif


// Path iterator structure
typedef struct di_path_iter {
    // Outputs
    int         index;      // 0-based component index

    const char* start;      // Pointer to the first character of the current path component. NULL if there is no more component
    const char* end;        // Pointer to the '/' or '\0' following the path component. NULL if there is no more component
    size_t      length;                     // Length of teh path component. 0 if there is no more component
    
    // Internal (do not modify)
    const char* _next_ptr;
} di_path_iter_t;


// Initialize a path iterator
#define DI_PATH_ITER_INITIALIZER {0, 0, 0, 0, 0}


#ifdef _WIN32
#define HOST_PATH_MAX   MAX_PATH
#else
#define HOST_PATH_MAX   PATH_MAX
#endif
#define DMG_PATH_MAX    1024        // kpi/syslimits.h
#define LPC_SIZE        FILENAME_MAX

#define COPY_BUF_SIZE   4096


extern char g_copy_buf[COPY_BUF_SIZE];
extern char g_host_path[HOST_PATH_MAX];
extern char g_dmg_path[DMG_PATH_MAX];
extern char g_last_path_comp[LPC_SIZE];



extern errno_t host_set_path(const char* path);
extern errno_t dmg_set_path(const char* path);


#ifdef _WIN32
#define host_is_directory(__path) \
win32_is_directory(__path)

#define host_path_next(__it, __path) \
win32_path_next(__it, __path)

#define host_path_append(__buf, __buf_size, __component) \
win32_path_append(__buf, __buf_size, __component)

#define host_last_path_component(__buf, __buf_size, __path) \
win32_last_path_component(__buf, __buf_size, __path)

#else

#define host_is_directory(__path) \
posix_is_directory(__path)

#define host_path_next(__it, __path) \
posix_path_next(__it, __path)

#define host_path_append(__buf, __buf_size, __component) \
posix_path_append(__buf, __buf_size, __component)

#define host_last_path_component(__buf, __buf_size, __path) \
posix_last_path_component(__buf, __buf_size, __path)
#endif


#define dmg_is_directory(__path) \
posix_is_host_directory(__path)

#define dmg_path_next(__it, __path) \
posix_path_next(__it, __path)

#define dmg_path_append(__buf, __buf_size, __component) \
posix_path_append(__buf, __buf_size, __component)

#define dmg_last_path_component(__buf, __buf_size, __path) \
posix_last_path_component(__buf, __buf_size, __path)



//
// Private
//

#ifdef _WIN32
// Windows host paths, with drive letters, UNC shares and device paths.
// The root ("C:\", "\\server\share\", ...) is a prefix of the first component
// that host_path_next() returns; a bare drive "C:" is a component of its own.

// The code page of narrow host paths: the one the C runtime uses for argv and
// fopen() (UTF-8 in a UTF-8 locale, otherwise the ANSI or OEM code page)
extern UINT win32_code_page(void);

extern bool win32_is_directory(const char* path);
extern int win32_path_next(di_path_iter_t* it, const char*  path);

extern int win32_path_append(char* buf, size_t buf_size, const char*  component);
extern int win32_last_path_component(char* buf, size_t buf_size, const char* path);
#endif /* _WIN32 */

extern bool posix_is_directory(const char*  path);
extern int posix_path_next(di_path_iter_t*  it, const char*  path);

extern int posix_path_append(char* buf, size_t buf_size, const char* component);
extern int posix_last_path_component(char* buf, size_t buf_size, const char* path);

#endif /* path_utils_h */
