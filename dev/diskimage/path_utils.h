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

/* What a path refers to */
enum {
    KIND_ERROR = -1,    /* it couldn't be determined; errno is set */
    KIND_NONE = 0,      /* nothing */
    KIND_DIRECTORY = 1,
    KIND_OTHER = 2      /* a file or anything else that isn't a directory */
};


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
extern char g_last_path_comp[LPC_SIZE];



extern errno_t host_validate_path(const char* path);
extern errno_t dmg_validate_path(const char* path);


#ifdef _WIN32
#define host_ends_with_separator(__path) \
win32_ends_with_separator(__path)

#define host_path_next(__it, __path) \
win32_path_next(__it, __path)

#define host_path_append(__buf, __buf_size, __component) \
win32_path_append(__buf, __buf_size, __component)

#define host_last_path_component(__buf, __buf_size, __path) \
win32_last_path_component(__buf, __buf_size, __path)


#define host_path_kind(__path) \
win32_path_kind(__path)

#define host_create_dir(__path, __created) \
win32_create_dir(__path, __created)

#define host_fopen(__path, __mode) \
win32_fopen(__path, __mode)

#else
#define host_ends_with_separator(__path) \
posix_ends_with_separator(__path)

#define host_path_next(__it, __path) \
posix_path_next(__it, __path)

#define host_path_append(__buf, __buf_size, __component) \
posix_path_append(__buf, __buf_size, __component)

#define host_last_path_component(__buf, __buf_size, __path) \
posix_last_path_component(__buf, __buf_size, __path)


#define host_path_kind(__path) \
posix_path_kind(__path)

#define host_create_dir(__path, __created) \
posix_create_dir(__path, __created)

#define host_fopen(__path, __mode) \
fopen(__path, __mode)

#endif


#define dmg_ends_with_separator(__path) \
posix_ends_with_separator(__path)

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

/*
 * Whether 'path' ends with a separator ('\' or '/'). In a DBCS code page
 * (932, 936, 949, 950) the second byte of a double-byte character is 0x40
 * or higher, so it can be 0x5C ('\') but never 0x2F ('/'). Only a final '\'
 * therefore needs a closer look: walking the characters from the start
 * tells whether it begins a character of its own. UTF-8 multi-byte
 * sequences never contain ASCII bytes, so there a final '\' is always one.
 */
extern bool win32_ends_with_separator(const char *path);
extern int win32_path_next(di_path_iter_t* it, const char*  path);

extern int win32_path_append(char* buf, size_t buf_size, const char*  component);
extern int win32_last_path_component(char* buf, size_t buf_size, const char* path);

extern int win32_path_kind(const char *path);
extern int win32_create_dir(const char *path, int *created);
extern FILE* win32_fopen(const char *path, const char* mode);
#endif /* _WIN32 */


extern bool posix_ends_with_separator(const char *path);
extern int posix_path_next(di_path_iter_t*  it, const char*  path);

extern int posix_path_append(char* buf, size_t buf_size, const char* component);
extern int posix_last_path_component(char* buf, size_t buf_size, const char* path);

#if defined(__APPLE__)
extern int posix_path_kind(const char *path);
extern int posix_create_dir(const char *path, int *created);
#endif /* __APPLE__ */

#endif /* path_utils_h */
