//
//  floatmath.c
//  libm
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <math.h>
#include <ext/math.h>

double fabs(double x)
{
    return __abs(x);
}

float fabsf(float x)
{
    return __abs(x);
}

long double fabsl(long double x)
{
    return __abs(x);
}
