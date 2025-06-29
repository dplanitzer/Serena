//
//  SELock.h
//  diskimage
//
//  Created by Dietmar Planitzer on 4/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_SELock_h
#define di_SELock_h

#include <windows.h>

enum {
    kSELState_Unlocked = 0,
    kSELState_LockedShared,
    kSELState_LockedExclusive
};

typedef struct SELock {
    SRWLOCK     lock;
    int         state;
} SELock;

extern void SELock_Init(SELock* self);
extern void SELock_Deinit(SELock* self);
extern errno_t SELock_LockShared(SELock* self);
extern errno_t SELock_LockExclusive(SELock* self);
extern errno_t SELock_Unlock(SELock* self);

#endif /* di_SELock_h */
