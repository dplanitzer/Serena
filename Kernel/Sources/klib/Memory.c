//
//  Memory.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Memory.h"

#if defined(__ILP32__)
typedef uint32_t uword_t;
#define WORD_SIZE       4
#define WORD_SIZMASK    3
#define WORD_SHIFT      2
#define WORD_FROM_BYTE(b) ((b) << 24) | ((b) << 16) | ((b) << 8) | (b)
#elif defined(__LLP64__) || defined(__LP64__)
typedef uint64_t uword_t;
#define WORD_SIZE       8
#define WORD_SIZMASK    7
#define WORD_SHIFT      3
#define WORD_FROM_BYTE(b) ((b) << 56) | ((b) << 48) | ((b) << 40) | ((b) << 32) | ((b) << 24) | ((b) << 16) | ((b) << 8) | (b)
#else
#error "unknown data model"
#endif


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

// Optimized version of memcpy_rev() which requires that 'src' and 'dst' pointers
// are aligned the same way. Meaning that
// (src & WORD_SIZMASK) == (dst & WORD_SIZMASK)
// holds true.
static void memcpy_opt_rev(void* _Nonnull _Restrict dst, const void* _Nonnull _Restrict src, size_t count)
{
    uint8_t* p = (uint8_t*)dst + count;
    const uint8_t* ps = (const uint8_t*)src + count;
    const uint8_t* const pe = (uint8_t*)dst;
  
    // Align to the next natural word size boundary
    const size_t n_mod_bytes = (uintptr_t)(p - 1) & WORD_SIZMASK;
    if (n_mod_bytes > 0) {
        do {
            *(--p) = *(--ps);
        } while ((uintptr_t)p & WORD_SIZMASK);

        count -= n_mod_bytes;
    }


    // We know that we can do at least one natural word size op at this point.
    // Unroll by a factor of 4 whenever possible
    uword_t* pw = (uword_t*)p;
    const uword_t* psw = (const uword_t*)ps;
    const uword_t* const pew = pw - (count >> WORD_SHIFT);
    size_t n_4s = (uintptr_t)(pw - pew) >> 2;
    while (n_4s > 0) {
        *(--pw) = *(--psw);
        *(--pw) = *(--psw);
        *(--pw) = *(--psw);
        *(--pw) = *(--psw);
        n_4s--;
    }
    while (pw > pew) {
        *(--pw) = *(--psw);
    }


    // Write the remaining bytes
    p = (uint8_t*)pw;
    ps = (const uint8_t*)psw;
    while (p > pe) {
        *(--p) = *(--ps);
    }
}

static void* _Nonnull memcpy_rev(void* _Nonnull _Restrict dst, const void* _Nonnull _Restrict src, size_t count)
{
    if (src == dst || count == 0) {
        return dst;
    }


    // Use the optimized version whenever possible
    if (count >= 2*WORD_SIZE && ((uintptr_t)src & WORD_SIZMASK) == ((uintptr_t)dst & WORD_SIZMASK)) {
        memcpy_opt_rev(dst, src, count);
        return dst;
    }


    // 8bit reverse copy
    uint8_t* p = (uint8_t*)dst + count;
    const uint8_t* ps = (const uint8_t*)src + count;
    const uint8_t* const pe = dst;

    size_t n_4s = count >> 2;
    while(n_4s > 0) {
        *(--p) = *(--ps);
        *(--p) = *(--ps);
        *(--p) = *(--ps);
        *(--p) = *(--ps);
        n_4s--;
    }

    while(p > pe) {
        *(--p) = *(--ps);
    }

    return dst;
}

// Copies 'count' contiguous bytes in memory from 'src' to 'dst'. The source and
// destination regions may overlap.
void* _Nonnull memmove(void* _Nonnull _Restrict dst, const void* _Nonnull _Restrict src, size_t count)
{
    if (dst < src) {
        memcpy(dst, src, count);
    }
    else {
        memcpy_rev(dst, src, count);
    }

    return dst;
}

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
