//
//  kern/floattypes.h
//  libc
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_FLOATTYPES_H
#define _KERN_FLOATTYPES_H 1

typedef unsigned short  float16_t;
typedef float           float32_t;
typedef double          float64_t;
typedef struct float96_t  { unsigned int w[3]; }  float96_t;
typedef struct float128_t { unsigned int w[4]; }  float128_t;

#endif /* _KERN_FLOATTYPES_H */
