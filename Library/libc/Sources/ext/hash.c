//
//  hash.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 3/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <ext/hash.h>


size_t hash_djb2_string(const char* _Nonnull str)
{
    size_t h = 5381;
    size_t c;

    while ((c = *str++) != '\0') {
        h = ((h << 5) + h) + c;
    }
    return h;
}

size_t hash_djb2_bytes(const void* _Nonnull bytes, size_t len)
{
    const char* sp = bytes;
    size_t h = 5381;

    while (len-- > 0) {
        h = ((h << 5) + h) + (size_t)*sp++;
    }
    return h;
}
