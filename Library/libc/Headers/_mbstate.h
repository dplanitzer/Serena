//
//  _mbstate.h
//  libc
//
//  Created by Dietmar Planitzer on 12/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_MBSTATE_H
#define _SYS_MBSTATE_H 1

#include <_cmndef.h>

__CPP_BEGIN

typedef struct mbstate {
    int dummy[4];
} mbstate_t;

__CPP_END

#endif /* _SYS_MBSTATE_H */
