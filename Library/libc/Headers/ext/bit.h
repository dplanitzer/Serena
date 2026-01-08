//
//  ext/bit.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 1/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_BIT_H
#define _EXT_BIT_H 1

#include <ext/limits.h>
#include <stdbool.h>

extern bool ispow2_ul(unsigned long n);
extern bool ispow2_ull(unsigned long long n);

#define ispow2_ui(__n)\
ispow2_ul((unsigned long)__n)

#if SIZE_WIDTH == 32
#define ispow2_sz(__n) \
ispow2_ul((unsigned long)__n)
#elif SIZE_WIDTH == 64
#define ispow2_sz(__n) \
ispow2_ull((unsigned long long)__n)
#else
#error "unable to define ispow2_sz()"
#endif


extern unsigned long pow2_ceil_ul(unsigned long n);
extern unsigned long long pow2_ceil_ull(unsigned long long n);

#define pow2_ceil_ui(__n)\
(unsigned int)pow2_ceil_ul((unsigned long)__n)

#if SIZE_WIDTH == 32
#define pow2_ceil_sz(__n) \
(size_t)pow2_ceil_ul((unsigned long)__n)
#elif SIZE_WIDTH == 64
#define pow2_ceil_sz(__n) \
(size_t)pow2_ceil_ull((unsigned long long)__n)
#else
#error "unable to define pow2_ceil_sz()"
#endif


extern unsigned int log2_ul(unsigned long n);
extern unsigned int log2_ull(unsigned long long n);

#define log2_ui(__n)\
log2_ul((unsigned long)__n)

#if SIZE_WIDTH == 32
#define log2_sz(__n) \
log2_ul((unsigned long)__n)
#elif SIZE_WIDTH == 64
#define log2_sz(__n) \
log2_ull((unsigned long long)__n)
#else
#error "unable to define log2_sz()"
#endif

#endif /* _EXT_BIT_H */
