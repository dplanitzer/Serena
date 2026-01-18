//
//  arch/floattypes.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _ARCH_FLOATTYPES_H
#define _ARCH_FLOATTYPES_H 1

typedef float           float32_t;
typedef double          float64_t;


//#define __FLOAT16__ 1
//typedef unsigned short  float16_t;

#if __M68K__
#define __FLOAT96__ 1
typedef struct float96_t  { unsigned int w[3]; }  float96_t;
#endif

//#define __FLOAT128__ 1
//typedef struct float128_t { unsigned int w[4]; }  float128_t;

#endif /* _ARCH_FLOATTYPES_H */
