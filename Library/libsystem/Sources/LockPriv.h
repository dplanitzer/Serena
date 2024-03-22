//
//  LockPriv.h
//  libsystem
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_LOCK_PRIV_H
#define _SYS_LOCK_PRIV_H 1

#include <System/Lock.h>

__CPP_BEGIN

#define LOCK_SIGNATURE 0x4c4f434b

// Must be sizeof(ULock) <= 16 
typedef struct ULock {
    int             od;
    unsigned int    signature;
    int             r2;
    int             r3;
} ULock;

__CPP_END

#endif /* _SYS_LOCK_PRIV_H */
