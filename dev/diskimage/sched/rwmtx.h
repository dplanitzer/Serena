//
//  rwmtx.h
//  diskimage
//
//  Created by Dietmar Planitzer on 4/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_rwmtx_h
#define di_rwmtx_h

#ifdef _WIN32
#include <windows.h>

enum {
    kSELState_Unlocked = 0,
    kSELState_LockedShared,
    kSELState_LockedExclusive
};

typedef struct rwmtx {
    SRWLOCK     lock;
    int         state;
} rwmtx_t;
#else
#include <errno.h>
#include <pthread.h>

typedef pthread_rwlock_t rwmtx_t;
#endif

extern void rwmtx_init(rwmtx_t* self);
extern void rwmtx_deinit(rwmtx_t* self);
extern errno_t rwmtx_rdlock(rwmtx_t* self);
extern errno_t rwmtx_wrlock(rwmtx_t* self);
extern errno_t rwmtx_unlock(rwmtx_t* self);

#endif /* di_rwmtx_h */
