//
//  float.h
//  libm
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _FLOAT_H
#define _FLOAT_H 1

#include <_dmdef.h>

#define DECIMAL_DIG 17

#define FLT_RADIX 2
#define FLT_MANT_DIG 24
#define FLT_EPSILON 1.19209290e-07f
#define FLT_DIG 6
#define FLT_MIN_EXP -125
#define FLT_MIN 1.17549435e-38f
#define FLT_MIN_10_EXP -37
#define FLT_MAX_EXP +128
#define FLT_MAX 3.40282347e+38f
#define FLT_MAX_10_EXP +38

#define DBL_MANT_DIG 53
#define DBL_EPSILON 2.2204460492503131e-16
#define DBL_DIG 15
#define DBL_MIN_EXP -1021
#define DBL_MIN 2.2250738585072014e-308
#define DBL_MIN_10_EXP -307
#define DBL_MAX_EXP +1024
#define DBL_MAX 1.7976931348623157e+308
#define DBL_MAX_10_EXP +308

#define FLT_EVAL_METHOD 0
// XXX update this once we properly initialize the FPU
#define FLT_ROUNDS -1

#endif /* _FLOAT_H */
