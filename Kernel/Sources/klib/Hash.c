//
//  Hash.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "Hash.h"


size_t hash_djb2_string(const char* _Nonnull str)
{
    size_t hash = 5381;

    while (*str != '\0') {
        hash = ((hash << 5) + hash) + (size_t)*str++;
    }
    return hash;
}

size_t hash_djb2_bytes(const void* _Nonnull bytes, size_t len)
{
    const char* sp = bytes;
    size_t hash = 5381;

    while (len-- > 0) {
        hash = ((hash << 5) + hash) + (size_t)*sp++;
    }
    return hash;
}
