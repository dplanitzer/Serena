//
//  dd_path.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef DD_PATH_H
#define DD_PATH_H

#include <stdbool.h>
#include <stdio.h>

// Path iterator structure
typedef struct dd_path_iter {
    // Outputs
    int         index;      // 0-based component index

    const char* start;      // Pointer to the first character of the current path component. NULL if there is no more component
    const char* end;        // Pointer to the '/' or '\0' following the path component. NULL if there is no more component
    size_t      length;                     // Length of teh path component. 0 if there is no more component
    
    // Internal (do not modify)
    const char* _next_ptr;
} dd_path_iter_t;


// Initialize a path iterator
#define DD_PATH_ITER_INITIALIZER {0, 0, 0, 0, 0}


#define DD_PATH_MAX    1024        // kpi/syslimits.h



extern errno_t dd_validate_path(const char* path);

extern bool dd_ends_with_separator(const char *path);
extern int dd_path_next(dd_path_iter_t*  it, const char*  path);

extern int dd_path_append(char* buf, size_t buf_size, const char* component);
extern int dd_last_path_component(char* buf, size_t buf_size, const char* path);

#endif /* DD_PATH_H */
