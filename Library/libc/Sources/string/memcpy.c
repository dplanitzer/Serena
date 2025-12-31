//
//  memcpy.c
//  libsc
//
//  Created by Dietmar Planitzer on 3/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <__string.h>


// Optimized version of memcpy() which requires that 'src' and 'dst' pointers
// are aligned the same way. Meaning that
// (src & WORD_SIZMASK) == (dst & WORD_SIZMASK)
// holds true.
static void memcpy_opt(void* _Nonnull _Restrict dst, const void* _Nonnull _Restrict src, size_t count)
{
    uint8_t* p = dst;
    const uint8_t* ps = src;
  
    // Align to the next natural word size boundary
    const size_t n_mod_bytes = (uintptr_t)p & WORD_SIZMASK;
    if (n_mod_bytes > 0) {
        do {
            *p++ = *ps++;
        } while ((uintptr_t)p & WORD_SIZMASK);

        count -= (WORD_SIZE - n_mod_bytes);
    }
    const uint8_t* const pe = p + count;


    // We know that we can do at least one natural word size op at this point.
    // Unroll by a factor of 4 whenever possible
    uword_t* pw = (uword_t*)p;
    const uword_t* psw = (const uword_t*)ps;
    const uword_t* const pew = pw + (count >> WORD_SHIFT);
    size_t n_4s = (uintptr_t)(pew - pw) >> 2;
    while (n_4s > 0) {
        *pw++ = *psw++;
        *pw++ = *psw++;
        *pw++ = *psw++;
        *pw++ = *psw++;
        n_4s--;
    }
    while (pw < pew) {
        *pw++ = *psw++;
    }


    // Write the remaining bytes
    p = (uint8_t*)pw;
    ps = (const uint8_t*)psw;
    while (p < pe) {
        *p++ = *ps++;
    }
}

void* _Nonnull memcpy(void* _Nonnull _Restrict dst, const void* _Nonnull _Restrict src, size_t count)
{
    if (src == dst || count == 0) {
        return dst;
    }


    // Use the optimized version whenever possible
    if (count >= 2*WORD_SIZE && ((uintptr_t)src & WORD_SIZMASK) == ((uintptr_t)dst & WORD_SIZMASK)) {
        memcpy_opt(dst, src, count);
        return dst;
    }


    // 8bit copy
    uint8_t* p = dst;
    const uint8_t* ps = src;
    const uint8_t* const pe = (uint8_t*)dst + count;
    size_t n_4s = count >> 2;

    while(n_4s > 0) {
        *p++ = *ps++;
        *p++ = *ps++;
        *p++ = *ps++;
        *p++ = *ps++;
        n_4s--;
    }

    while(p < pe) {
        *p++ = *ps++;
    }

    return dst;
}
