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

extern void SEmtx_init(SELock* self);
extern void SEmtx_deinit(SELock* self);
extern errno_t SEmtx_lock_Shared(SELock* self);
extern errno_t SEmtx_lock_Exclusive(SELock* self);
extern errno_t SEmtx_unlock(SELock* self);

#endif /* di_SELock_h */
