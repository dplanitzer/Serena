//
//  _mbstate.h
//  libc
//
//  Created by Dietmar Planitzer on 12/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef __MBSTATE_H
#define __MBSTATE_H 1

#if ___STDC_HOSTED__ != 1
#error "not supported in freestanding mode"
#endif

#include <_cmndef.h>

__CPP_BEGIN

typedef struct mbstate {
    int dummy[4];
} mbstate_t;

__CPP_END

#endif /* __MBSTATE_H */
