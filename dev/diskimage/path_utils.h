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


#define DMG_PATH_MAX    1024        // kpi/syslimits.h



extern errno_t dmg_validate_path(const char* path);


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


extern bool posix_ends_with_separator(const char *path);
extern int posix_path_next(di_path_iter_t*  it, const char*  path);

extern int posix_path_append(char* buf, size_t buf_size, const char* component);
extern int posix_last_path_component(char* buf, size_t buf_size, const char* path);

#endif /* path_utils_h */
