//
//  filemem.c
//  libc
//
//  Created by Dietmar Planitzer on 1/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


int __filemem(FILE * _Nonnull _Restrict s, FILE_MemoryQuery * _Nonnull _Restrict query)
{
    if (s->cb.read == (FILE_Read)__mem_read) {
        const __Memory_FILE_Vars* mp = (__Memory_FILE_Vars*)s->context;

        query->base = mp->store;
        query->eof = mp->eofPosition;
        query->capacity = mp->currentCapacity;
        return 0;
    }
    else {
        query->base = NULL;
        query->eof = 0;
        query->capacity = 0;
        return EOF;
    }
}

int filemem(FILE * _Nonnull _Restrict s, FILE_MemoryQuery * _Nonnull _Restrict query)
{
    __flock(s);
    const int r = __filemem(s, query);
    __funlock(s);
    
    return r;
}
