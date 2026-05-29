//
//  fp_tests.c
//  C Tests
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


    printf("\n---\n");


    // uin32_t -> float, double
    volatile uint32_t ui0 = UINT32_MAX;
    float f0 = (float)ui0;
    double d0 = (double)ui0;
    printf("uint32: %u -> f32: %f -> f64: %f\n", ui0, f0, d0);


    // float, double -> uin32_t
    volatile float f1 = 4294967295.0f;
    volatile double d1 = 4294967295.0;
    uint32_t ui1 = (uint32_t)f1;
    uint32_t ui2 = (uint32_t)d1;
    printf("f32: %f -> uint32: %u, f64: %f -> uint32: %u\n", f1, ui1, d1, ui2);


    // in32_t -> float, double
    volatile int32_t i0 = INT32_MAX;
    f0 = (float)i0;
    d0 = (double)i0;
    printf("int32: %d -> f32: %f -> f64: %f\n", i0, f0, d0);


    // float, double -> in32_t
    f1 = 2147483647.0f;
    d1 = 2147483647.0;
    int32_t i1 = (int32_t)f1;
    int32_t i2 = (int32_t)d1;
    printf("f32: %f -> int32: %d, f64: %f -> int32: %d\n", f1, i1, d1, i2);


    printf("\n---\n");


    // uint64_t -> double
    volatile uint64_t ui3 = 4294968296;
    d0 = (double)ui3;
    printf("uint64: %llu -> f64: %f\n", ui3, d0);


    // int64_t -> double
    volatile int64_t i3 = 4294968296;
    volatile int64_t i4 = -4294968296;
    d0 = (double)i3;
    d1 = (double)i4;
    printf("int64: %lld -> f64: %f;  int64: %lld -> f64: %f\n", i3, d0, i4, d1);
}
