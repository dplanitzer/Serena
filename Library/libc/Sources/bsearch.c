//
//  bsearch.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <__stddef.h>


// <https://en.wikipedia.org/wiki/Binary_search_algorithm>
void* bsearch(const void *key, const void *ptr, size_t count, size_t size, int (*comp)(const void*, const void*))
{
    if (count > 0 && count <= __SSIZE_MAX) {
        // Okay, sorry gotta complain. This if(count > 0) is only necessary because
        // size_t is guaranteed to be an unsigned integral type. Repeat after me
        // and learn an important lesson in life: types which are going to be used 
        // with math operations, like minus should never, ever be unsigned.
        // Why?
        // Because it makes it really easy to end up with accidental wrap arounds
        // if the lhs is smaller than the rhs. That's why you are then forced to
        // sprinkle your code with these extra tests to catch these cases before
        // they blow up in our face.
        // An unsigned integral type should only ever be used to represent bit
        // sets aka masks. But never anything that we are going to use to do
        // run-of-the-mill math with.
        // You'd think that after about 50 to 60 years people would have figured
        // that out. But, nope. Too obvious a way to stick a bug into your code
        // that then causes security issues and we all just love those, right?
        const char* p = (const char*)ptr;
        ssize_t l = 0;
        ssize_t r = ((ssize_t) count) - 1;

        while (l <= r) {
            const ssize_t m = (l + r) >> 1;
            const char* mptr = &p[m * size];
            const int t = comp(key, mptr);

            if (t > 0) {
                l++;
            } else if (t < 0) {
                r--;
            } else {
                return (void*)mptr;
            }
        }
    }

    return NULL;
}
