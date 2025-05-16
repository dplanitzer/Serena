//
//  _floattypes.h
//  libc
//
//  Created by Dietmar Planitzer on 2/8/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef __ABI_FLOATTYPES_H
#define __ABI_FLOATTYPES_H 1

#include <kern/_cmndef.h>
#include <machine/abi/_dmdef.h>


typedef unsigned short  float16_t;
typedef float           float32_t;
typedef double          float64_t;
typedef struct float96_t  { unsigned int w[3]; }  float96_t;
typedef struct float128_t { unsigned int w[4]; }  float128_t;

#endif /* __ABI_FLOATTYPES_H */
