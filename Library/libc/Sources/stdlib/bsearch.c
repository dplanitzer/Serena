//
//  bsearch.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <__stddef.h>


// <https://en.wikipedia.org/wiki/Binary_search_algorithm>
void* bsearch(const void *key, const void *ptr, size_t count, size_t size, int (*comp)(const void*, const void*))
{
    if (count == 0 || size == 0 || comp == NULL || key == NULL || ptr == NULL) {
        return NULL;
    }

    size_t l = 0;
    size_t r = count;

    while (l < r) {
        const size_t m = (l + r) >> 1;
        const char* mptr = ((const char*)ptr) + (m * size);
        const int t = comp(key, mptr);

        if (t < 0) {
            r = m;
        }
        else if (t > 0) {
            l = m + 1;
        }
        else {
            return (void*)mptr;
        }
    }

    return NULL;
}
