//
//  pathnext.c
//  libc
//
//  Created by Dietmar Planitzer on 10/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/path.h>
#include <stddef.h>

int pathnext(path_iter_t* _Nonnull it, const char* _Nonnull path)
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
