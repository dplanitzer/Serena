//
//  hd_path.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef HD_PATH_H
#define HD_PATH_H

#include <stdbool.h>
#include <stddef.h>

#define HD_PATH_MAX     4096


// Path iterator structure
typedef struct hd_path_iter {
    // Outputs
    int         index;      // 0-based component index

    const char* start;      // Pointer to the first character of the current path component. NULL if there is no more component
    const char* end;        // Pointer to the '/' or '\0' following the path component. NULL if there is no more component
    size_t      length;                     // Length of teh path component. 0 if there is no more component
    
    // Internal (do not modify)
    const char* _next_ptr;
} hd_path_iter_t;


// Initialize a path iterator
#define HD_PATH_ITER_INITIALIZER {0, 0, 0, 0, 0}


extern errno_t hd_validate_path(const char* path);

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
extern bool hd_ends_with_separator(const char *path);

extern int hd_path_next(hd_path_iter_t* it, const char* path);
extern int hd_path_append(char* buf, size_t buf_size, const char*  component);
extern int hd_last_path_component(char* buf, size_t buf_size, const char* path);

#endif /* HD_PATH_H */
