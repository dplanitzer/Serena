//
//  memset.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 3/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <__string.h>


#if ___STDC_HOSTED__ == 1
#include <__stddef.h>

void * _Nonnull memset(void * _Nonnull dst, int c, size_t count)
{
    return ((void* (*)(void*, int, size_t))__gProcessArguments->urt_funcs[KEI_memset])(dst, c, count);
}

#else

void* _Nonnull memset(void* _Nonnull dst, int c, size_t count)
{
    uint8_t* p = dst;
    const uint8_t b = (uint8_t)c;

    // Don't bother optimizing too small requests. It would take more time to
    // handle misalignments, unrolling and trailing bytes than it takes to
    // bite the bullet and process individual bytes
    if (count < 16) {
        const uint8_t* const pe = p + count;

        while (p < pe) {
            *p++ = b;
        }
        return dst;
    }


    // Align to the next natural word size boundary
    const size_t n_mod_bytes = ((uintptr_t)p & WORD_SIZMASK);
    if (n_mod_bytes > 0) {
        do {
            *p++ = b;
        } while ((uintptr_t)p & WORD_SIZMASK);

        count -= (WORD_SIZE - n_mod_bytes);
    }
    const uint8_t* const pe = p + count;


    // We know that we can do at least one natural word size op at this point.
    // Unroll by a factor of 4 whenever possible
    uword_t* pw = (uword_t*)p;
    const uword_t bw = WORD_FROM_BYTE(b);
    const uword_t* const pew = pw + (count >> WORD_SHIFT);
    size_t n_4s = (uintptr_t)(pew - pw) >> 2;
    while (n_4s > 0) {
        *pw++ = bw;
        *pw++ = bw;
        *pw++ = bw;
        *pw++ = bw;
        n_4s--;
    }
    while (pw < pew) {
        *pw++ = bw;
    }


    // Write the remaining bytes
    p = (uint8_t*)pw;
    while (p < pe) {
        *p++ = b;
    }

    return dst;
}

#endif
