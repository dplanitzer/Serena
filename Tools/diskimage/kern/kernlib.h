//
//  kern/kernlib.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_KERNLIB_H
#define _KERN_KERNLIB_H 1

#include <_cmndef.h>
#include <inttypes.h>   // imaxabs(), imaxdiv()
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h> // abs(), div(), qsort(), bsearch(), itoa(), ltoa(), strtol()
#include <ext/limits.h>
#include <../../../Kernel/Headers/kpi/_access.h>
#include <../../../Kernel/Headers/kpi/_seek.h>


//
// NOTE: All kernel related code must be compiled with a pure C environment. Eg
// no POSIX extensions (since those extensions would collide with our own POSIX-
// like definitions).
//
// Windows: compile with /std:c11
//


#define __abs(x) (((x) < 0) ? -(x) : (x))
#define __clamped(v, lw, up) ((v) < (lw) ? (lw) : ((v) > (up) ? (up) : (v)))

#define __Ceil_PowerOf2(x, __align)   (((x) + ((__align)-1)) & ~((__align)-1))
#define __Floor_PowerOf2(x, __align) ((x) & ~((__align)-1))

#define __Ceil_Ptr_PowerOf2(x, __align)     (void*)(__Ceil_PowerOf2((__uintptr_t)(x), (__uintptr_t)(__align)))
#define __Floor_Ptr_PowerOf2(x, __align)     (void*)(__Floor_PowerOf2((__uintptr_t)(x), (__uintptr_t)(__align)))


#define CHAR_PTR_MAX    ((char*)__UINTPTR_MAX)


// Convert a size_t to a ssize_t with clamping
#define __SSizeByClampingSize(ub) (ssize_t)__min(ub, SSIZE_MAX)


// Minimum size for types
// (Spelled out in a comment because C is stupid and doesn't give us a simple & easy
// way to statically compare the size of two types. Supporting that would be just
// too easy you know and the language only has had like 50 years to mature...)
// 
// uid_t        <= int
// gid_t        <= int
// errno_t      <= int
// pid_t        <= int
// fsid_t       <= int


// Size constructors
#define SIZE_GB(x)  ((unsigned long)(x) * 1024ul * 1024ul * 1024ul)
#define SIZE_MB(x)  ((long)(x) * 1024 * 1024)
#define SIZE_KB(x)  ((long)(x) * 1024)


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


// Required minimum size is (string length byte + sign byte + longest digit sequence + 1 NUL byte) -> 1 + 64 (binary 64bit) + 1 + 1 = 25 bytes
// A digit string is generated in a canonical representation: string length, sign, digits ..., NUL
#define DIGIT_BUFFER_CAPACITY 67

__CPP_END

#endif /* _KERN_KERNLIB_H */
