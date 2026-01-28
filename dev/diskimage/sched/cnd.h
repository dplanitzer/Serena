//
//  cnd.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_ConditionVariable_h
#define di_ConditionVariable_h

#include <ext/try.h>
#include <sched/mtx.h>
#include <time.h>

#ifdef _WIN32
typedef CONDITION_VARIABLE cnd_t;
#else
typedef pthread_cond_t cnd_t;
#endif

extern void cnd_init(cnd_t* pCondVar);
extern void cnd_deinit(cnd_t* pCondVar);
extern void cnd_signal(cnd_t* pCondVar);
extern void cnd_broadcast(cnd_t* pCondVar);
extern errno_t cnd_wait(cnd_t* pCondVar, mtx_t* mtx);
extern errno_t cnd_timedwait(cnd_t* pCondVar, mtx_t* mtx, const struct timespec* _Nonnull deadline);

#endif /* di_ConditionVariable_h */
