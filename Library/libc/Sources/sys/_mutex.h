//
//  sys/_mutex.h
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_PRIV_MUTEX_H
#define _SYS_PRIV_MUTEX_H 1

#include <sys/mutex.h>

__CPP_BEGIN

#define MUTEX_SIGNATURE 0x4c4f434b

// Must be sizeof(UMutex) <= 16 
typedef struct UMutex {
    int             od;
    unsigned int    signature;
    int             r2;
    int             r3;
} UMutex;

__CPP_END

#endif /* _SYS_PRIV_MUTEX_H */
