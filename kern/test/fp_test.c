//
//  fp_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 2/6/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Asserts.h"


// This test requires an FPU
void fp_test(int argc, char *argv[])
{
    static char buf[128];
    char* r = buf;
    int decpt; 
    int sign;
    double pi = 1e23; //3.1415926535;


    // strtod()
    pi = strtod("-3.1415926535", NULL);

    // dtoa()
    char* fr = dtoa(pi, 1, 8, &decpt, &sign, &r);
    printf("%s, decpt: %d, sign: %d\n", fr, decpt, sign);

    //assertEquals(0, abs(0));
}
