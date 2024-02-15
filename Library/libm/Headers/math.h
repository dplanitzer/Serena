//
//  math.h
//  libm
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _MATH_H
#define _MATH_H 1

#include <System/_cmndef.h>

__CPP_BEGIN

typedef float float_t;
typedef double double_t;

#define HUGE_VALF   1e50f
#define HUGE_VAL    1e500
#define HUGE_VALL   1e5000l

#define INFINITY    HUGE_VALF

// XXX not yet
//#define NAN


#define FP_INFINITE     1
#define FP_NAN          2
#define FP_NORMAL       3
#define FP_SUBNORMAL    4
#define FP_ZERO         5


#define MATH_ERRNO      1
#define MATH_ERREXCEPT  2
#define math_errhandling    (MATH_ERRNO|MATH_ERREXCEPT)


extern double fabs(double x);
extern float fabsf(float x);
extern long double fabsl(long double x);

__CPP_END

#endif /* _MATH_H */
