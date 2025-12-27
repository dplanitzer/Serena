//
//  ext/math.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 9/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_MATH_H
#define _EXT_MATH_H 1

#include <ext/limits.h>
#include <stdbool.h>

#define __abs(x) (((x) < 0) ? -(x) : (x))

#ifndef __MIN_MAX_DEFINED
#define __min(x, y) (((x) < (y) ? (x) : (y)))
#define __max(x, y) (((x) > (y) ? (x) : (y)))
#define __MIN_MAX_DEFINED 1
#endif

#define __clamped(v, lw, up) ((v) < (lw) ? (lw) : ((v) > (up) ? (up) : (v)))


#define __Ceil_PowerOf2(x, __align)   (((x) + ((__align)-1)) & ~((__align)-1))
#define __Floor_PowerOf2(x, __align) ((x) & ~((__align)-1))

#define __Ceil_Ptr_PowerOf2(x, __align)     (void*)(__Ceil_PowerOf2((__uintptr_t)(x), (__uintptr_t)(__align)))
#define __Floor_Ptr_PowerOf2(x, __align)     (void*)(__Floor_PowerOf2((__uintptr_t)(x), (__uintptr_t)(__align)))


extern bool ul_ispow2(unsigned long n);
extern bool ull_ispow2(unsigned long long n);

#define u_ispow2(__n)\
ul_ispow2((unsigned long)__n)

#if SIZE_WIDTH == 32
#define siz_ispow2(__n) \
ul_ispow2((unsigned long)__n)
#elif SIZE_WIDTH == 64
#define siz_ispow2(__n) \
ull_ispow2((unsigned long long)__n)
#else
#error "unable to define siz_ispow2()"
#endif


extern unsigned long ul_pow2_ceil(unsigned long n);
extern unsigned long long ull_pow2_ceil(unsigned long long n);

#define u_pow2_ceil(__n)\
(unsigned int)ul_pow2_ceil((unsigned long)__n)

#if SIZE_WIDTH == 32
#define siz_pow2_ceil(__n) \
(size_t)ul_pow2_ceil((unsigned long)__n)
#elif SIZE_WIDTH == 64
#define siz_pow2_ceil(__n) \
(size_t)ull_pow2_ceil((unsigned long long)__n)
#else
#error "unable to define siz_pow2_ceil()"
#endif


extern unsigned int ul_log2(unsigned long n);
extern unsigned int ull_log2(unsigned long long n);

#define u_log2(__n)\
ul_log2((unsigned long)__n)

#if SIZE_WIDTH == 32
#define siz_log2(__n) \
ul_log2((unsigned long)__n)
#elif SIZE_WIDTH == 64
#define siz_log2(__n) \
ull_log2((unsigned long long)__n)
#else
#error "unable to define siz_log2()"
#endif

#endif /* _EXT_MATH_H */
