//
//  fp_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 2/6/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


// This test requires an FPU
void fp_test(int argc, char *argv[])
{
    double pi = 3.1415926535;

    // strtod()
    double pi_neg = strtod("-3.1415926535", NULL);

    // printf(%f, %e, %g)
    printf("pi: %.3f, pi_neg: %f\n", pi, pi_neg);
    printf("pi: %e, FLT_MAX: %E\n", pi, FLT_MAX);


    // sqrt()
    printf("sqrt(%g) -> %g\n", 64.0, sqrt(64.0));
//    printf("sqrt(%g) -> %g\n", -64.0, sqrt(-64.0));
//    printf("sqrt(+0.0) -> %g, sqrt(-0.0) -> %g\n", sqrt(0.0), sqrt(-0.0));
//    printf("sqrt(nan) -> %g, sqrt(-inf) -> %g\n", sqrt(NAN), sqrt(-INFINITY));
}
