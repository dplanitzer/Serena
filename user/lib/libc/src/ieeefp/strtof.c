//
//  strtof.c
//  lib
//
//  Created by Dietmar Planitzer on 2/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>


float strtof(const char * _Restrict str, char ** _Restrict str_end)
{
    const double dr = strtod(str, str_end);
    float r;

    if (dr < -FLT_MAX) {
        r = -HUGE_VALF;
        errno = ERANGE;
    }
    else if (dr > FLT_MAX) {
        r = HUGE_VALF;
        errno = ERANGE;
    }
    else {
        //XXX check with denormalized numbers, (S)NaN, Inf
        r = (float)dr;
    }

    return r;
}
